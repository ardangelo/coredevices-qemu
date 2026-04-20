/*-
 * Copyright (c) 2013
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * QEMU Sharp Memory LCD device model.
 * Supports:
 *  - LS013B7DH01 (144x168)
 *  - LS027B7DH01 (400x240)
 */

#include "qemu-common.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "hw/ssi.h"

/* Cyberdeck-specific display mux extensions */
#define BOARD_SILK_CYBERDECK
#ifdef BOARD_SILK_CYBERDECK
#include "sysemu/char.h"
#endif

typedef enum {
    COMMAND,
    LINENO,
    DATA,
    TRAILER
} xfer_state_t;

typedef struct {
    SSISlave ssidev;
    QemuConsole *con;
    bool redraw;
    uint8_t *framebuffer;
    int fbindex;
    xfer_state_t state;

    bool   backlight_enabled;
    float  brightness;

    bool   vibrate_on;
    int    vibrate_offset;

    bool power_on;

    /* Tintin display was installed 'upside-down'.
     * Use the "rotate_display" property to flip it.
     */
    bool rotate_display;

    /* Configurable dimensions */
    uint32_t num_rows;
    uint32_t num_cols;

#ifdef BOARD_SILK_CYBERDECK
    /* K230 (Linux) framebuffer and protocol state */
    uint8_t *k230_fb;
    int      k230_fbindex;
    xfer_state_t k230_state;
    CharDriverState *k230_chr;
    bool     lcd_sel_k230;   /* false = nRF (default), true = K230 */
#endif
} lcd_state;

static uint8_t
bitswap(uint8_t val)
{
    return ((val * 0x0802LU & 0x22110LU) | (val * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
}

static void
sm_lcd_reset_protocol_state(lcd_state *s)
{
    s->fbindex = 0;
    s->state = COMMAND;
#ifdef BOARD_SILK_CYBERDECK
    s->k230_fbindex = 0;
    s->k230_state = COMMAND;
#endif
}

/*
 * Core Sharp protocol state machine, shared by the SPI path (sm_lcd_transfer)
 * and the K230 chardev path (sm_lcd_k230_receive). Returns true if a redraw
 * is needed.
 */
static bool
sm_lcd_process_byte(uint8_t data, uint8_t *fb, int *fbindex,
                    xfer_state_t *state,
                    uint32_t num_col_bytes, uint32_t fb_size)
{
    bool need_redraw = false;
    switch (*state) {
    case COMMAND:
        data &= 0xfd; /* Mask VCOM bit */
        switch (data) {
        case 0x01: /* Write Line */
            *state = LINENO;
            break;
        case 0x04: /* Clear Screen */
            if (fb) {
                memset(fb, 0, fb_size);
            }
            need_redraw = true;
            break;
        case 0x00: /* Toggle VCOM */
            break;
        default:
            /* Simulate confused display controller. */
            if (fb) {
                memset(fb, 0x55, fb_size);
            }
            need_redraw = true;
            break;
        }
        break;
    case LINENO:
        if (data == 0) {
            *state = COMMAND;
        } else {
            *fbindex = (data - 1) * num_col_bytes;
            *state = DATA;
        }
        break;
    case DATA:
        if (fb && *fbindex < (int)fb_size) {
            fb[(*fbindex)++] = data;
        }
        if (*fbindex % num_col_bytes == 0) {
            *state = TRAILER;
        }
        break;
    case TRAILER:
        if (data != 0) {
            qemu_log_mask(LOG_GUEST_ERROR,
              "sharp memory lcd received non-zero data in TRAILER\n");
        }
        *state = LINENO;
        need_redraw = true;
        break;
    }
    return need_redraw;
}

static uint32_t
sm_lcd_transfer(SSISlave *dev, uint32_t data)
{
    lcd_state *s = FROM_SSI_SLAVE(lcd_state, dev);
    /* XXX QEMU's SPI infrastructure is implicitly MSB-first */
    uint32_t num_col_bytes = s->num_cols / 8;
    if (sm_lcd_process_byte(bitswap(data), s->framebuffer, &s->fbindex, &s->state,
                            num_col_bytes, s->num_rows * num_col_bytes)) {
#ifdef BOARD_SILK_CYBERDECK
        if (!s->lcd_sel_k230)
#endif
        s->redraw = true;
    }
    return 0;
}

static void sm_lcd_update_display(void *arg)
{
    lcd_state *s = arg;

#ifdef BOARD_SILK_CYBERDECK
    uint8_t *fb = s->lcd_sel_k230 ? s->k230_fb : s->framebuffer;
#else
    uint8_t *fb = s->framebuffer;
#endif
    if (!fb) {
        return;
    }

    uint8_t *d;
    uint32_t colour_on, colour_off, colour;
    int x, y, bpp;

    DisplaySurface *surface = qemu_console_surface(s->con);
    bpp = surface_bits_per_pixel(surface);
    d = surface_data(surface);


    // If vibrate is on, simply jiggle the display
    if (s->vibrate_on) {
        if (s->vibrate_offset == 0) {
            s->vibrate_offset = 2;
        }
        int bytes_per_pixel;
        switch (bpp) {
            case 8:
                bytes_per_pixel = 1;
                break;
            case 15:
            case 16:
                bytes_per_pixel = 2;
                break;
            case 32:
                bytes_per_pixel = 4;
                break;
            default:
                abort();
        }
        int total_bytes = s->num_rows * s->num_cols * bytes_per_pixel
                        - abs(s->vibrate_offset) * bytes_per_pixel;
        if (s->vibrate_offset > 0) {
            memmove(d, d + s->vibrate_offset * bytes_per_pixel, total_bytes);
        } else {
            memmove(d - s->vibrate_offset * bytes_per_pixel, d, total_bytes);
        }
        s->vibrate_offset *= -1;
        dpy_gfx_update(s->con, 0, 0, s->num_cols, s->num_rows);
        return;
    }

    if (!s->redraw) {
        return;
    }

    // Adjust the white level to compensate for the set brightness.
    // brightness = 0:  255 in maps to 170 out
    // brightness = 1.0: 255 in maps to 255 out
    float brightness = s->backlight_enabled ? s->brightness : 0.0;
    int max_val = 170 + (255 - 170) * brightness;

    /* set colours according to bpp */
    switch (bpp) {
    case 8:
        colour_on = rgb_to_pixel8(max_val, max_val, max_val);
        colour_off = rgb_to_pixel8(0x00, 0x00, 0x00);
        break;
    case 15:
        colour_on = rgb_to_pixel15(max_val, max_val, max_val);
        colour_off = rgb_to_pixel15(0x00, 0x00, 0x00);
        break;
    case 16:
        colour_on = rgb_to_pixel16(max_val, max_val, max_val);
        colour_off = rgb_to_pixel16(0x00, 0x00, 0x00);
    case 24:
        colour_on = rgb_to_pixel24(max_val, max_val, max_val);
        colour_off = rgb_to_pixel24(0x00, 0x00, 0x00);
        break;
    case 32:
        colour_on = rgb_to_pixel32(max_val, max_val, max_val);
        colour_off = rgb_to_pixel32(0x00, 0x00, 0x00);
        break;
    default:
        return;
    }

    uint32_t num_col_bytes = s->num_cols / 8;

    for (y = 0; y < s->num_rows; y++) {
        for (x = 0; x < s->num_cols; x++) {
            /* Rotate the display if necessary */
            int xr = (s->rotate_display) ? s->num_cols - 1 - x : x;
            int yr = (s->rotate_display) ? s->num_rows - 1 - y : y;
            bool on = fb[yr * num_col_bytes + xr / 8] & 1 << (xr % 8);
            colour = on ? colour_on : colour_off;
            switch(bpp) {
                case 8:
                    *((uint8_t *)d) = colour;
                    d++;
                    break;
                case 15:
                case 16:
                    *((uint16_t *)d) = colour;
                    d += 2;
                    break;
                case 24:
                    abort();
                case 32:
                    *((uint32_t *)d) = colour;
                    d += 4;
                    break;
            }
        }
    }

    dpy_gfx_update(s->con, 0, 0, s->num_cols, s->num_rows);
    s->redraw = false;
}

static void sm_lcd_invalidate_display(void *arg)
{
    lcd_state *s = arg;
    s->redraw = true;
}


// ----------------------------------------------------------------------------- 
static void sm_lcd_backlight_enable_cb(void *opaque, int n, int level)
{
    lcd_state *s = (lcd_state *)opaque;
    assert(n == 0);

    bool enable = (level != 0);
    if (s->backlight_enabled != enable) {
        s->backlight_enabled = enable;
        s->redraw = true;
    }
}


// -----------------------------------------------------------------------------
// Set brightness, from 0 to 255
static void sm_lcd_set_backlight_level_cb(void *opaque, int n, int level)
{
    lcd_state *s = (lcd_state *)opaque;
    assert(n == 0);

    float bright_f = (float)level / 255;

    // Temp hack - the Pebble sets the PWM to 25% for max brightness
    float new_setting = MIN(1.0, bright_f * 4);
    if (new_setting != s->brightness) {
        s->brightness = MIN(1.0, bright_f * 4);
        if (s->backlight_enabled) {
            s->redraw = true;
        }
    }
}


// ----------------------------------------------------------------------------- 
static void sm_lcd_vibe_ctl(void *opaque, int n, int level)
{
    lcd_state *s = (lcd_state *)opaque;
    assert(n == 0);

    s->vibrate_on = (level != 0);
}

// ----------------------------------------------------------------------------- 
static void sm_lcd_power_ctl(void *opaque, int n, int level)
{
    lcd_state *s = (lcd_state *)opaque;
    assert(n == 0);

    if (!level && s->power_on) {
        if (s->framebuffer) {
            uint32_t fb_size = s->num_rows * (s->num_cols / 8);
            memset(s->framebuffer, 0, fb_size);
#ifdef BOARD_SILK_CYBERDECK
            if (s->k230_fb) memset(s->k230_fb, 0, fb_size);
#endif
        }
        sm_lcd_reset_protocol_state(s);
        s->redraw = true;
        s->power_on = false;
    }
    s->power_on = !!level;
}


#ifdef BOARD_SILK_CYBERDECK

// -----------------------------------------------------------------------------
static void sm_lcd_lcd_sel_cb(void *opaque, int n, int level)
{
    lcd_state *s = (lcd_state *)opaque;
    s->lcd_sel_k230 = (level != 0);
    s->redraw = true;
}

static int sm_lcd_k230_can_receive(void *opaque)
{
    lcd_state *s = (lcd_state *)opaque;
    /*
     * A full multi-line Sharp write is:
     *   command + num_rows * (line tag + row data + trailer) + final trailer
     */
    return 2 + s->num_rows * ((s->num_cols / 8) + 2);
}

static void sm_lcd_k230_receive(void *opaque, const uint8_t *buf, int size)
{
    lcd_state *s = (lcd_state *)opaque;
    uint32_t ncb = s->num_cols / 8;
    uint32_t fb_size = s->num_rows * ncb;
    for (int i = 0; i < size; i++) {
        if (sm_lcd_process_byte(bitswap(buf[i]), s->k230_fb, &s->k230_fbindex,
                                &s->k230_state, ncb, fb_size)) {
            if (s->lcd_sel_k230) {
                s->redraw = true;
            }
        }
    }
}

static void sm_lcd_k230_event(void *opaque, int event) {}

#endif /* BOARD_SILK_CYBERDECK */

static void sm_lcd_reset(DeviceState *dev)
{
    lcd_state *s = (lcd_state *)dev;
    if (s->framebuffer) {
        uint32_t fb_size = s->num_rows * (s->num_cols / 8);
        memset(s->framebuffer, 0, fb_size);
#ifdef BOARD_SILK_CYBERDECK
        if (s->k230_fb) memset(s->k230_fb, 0, fb_size);
#endif
    }
    sm_lcd_reset_protocol_state(s);
    s->redraw = true;
}


// ----------------------------------------------------------------------------- 
static const GraphicHwOps sm_lcd_ops = {
    .gfx_update = sm_lcd_update_display,
    .invalidate = sm_lcd_invalidate_display,
};

static int sm_lcd_init(SSISlave *dev)
{
    lcd_state *s = FROM_SSI_SLAVE(lcd_state, dev);

    s->brightness = 0.0;
    sm_lcd_reset_protocol_state(s);

    s->con = graphic_console_init(DEVICE(dev), 0, &sm_lcd_ops, s);
    qemu_console_resize(s->con, s->num_cols, s->num_rows);

    /* Allocate framebuffer */
    uint32_t num_col_bytes = s->num_cols / 8;
    uint32_t fb_size = s->num_rows * num_col_bytes;
    s->framebuffer = g_malloc0(fb_size);

#ifdef BOARD_SILK_CYBERDECK
    s->k230_fb    = g_malloc0(fb_size);
    if (s->k230_chr) {
        qemu_chr_add_handlers(s->k230_chr, sm_lcd_k230_can_receive,
                              sm_lcd_k230_receive, sm_lcd_k230_event, s);
    }
#endif

    /* This callback informs us that brightness control is enabled */
    qdev_init_gpio_in_named(DEVICE(dev), sm_lcd_backlight_enable_cb,
                            "backlight_enable", 1);

    /* This callback informs us of the brightness level (from 0 to 255) */
    qdev_init_gpio_in_named(DEVICE(dev), sm_lcd_set_backlight_level_cb,
                            "backlight_level", 1);

    /* This callback informs us that the vibrate is on/orr */
    qdev_init_gpio_in_named(DEVICE(dev), sm_lcd_vibe_ctl,
                            "vibe_ctl", 1);

    /* This callback informs us that power is on/off */
    qdev_init_gpio_in_named(DEVICE(dev), sm_lcd_power_ctl,
                            "power_ctl", 1);

#ifdef BOARD_SILK_CYBERDECK
    /* This callback selects which source drives the display (0=nRF, 1=K230) */
    qdev_init_gpio_in_named(DEVICE(dev), sm_lcd_lcd_sel_cb,
                            "lcd_sel", 1);
#endif

    return 0;
}

static Property sm_lcd_properties[] = {
    DEFINE_PROP_BOOL("rotate_display", lcd_state, rotate_display, true),
    DEFINE_PROP_UINT32("num_rows", lcd_state, num_rows, 168),
    DEFINE_PROP_UINT32("num_cols", lcd_state, num_cols, 144),
    DEFINE_PROP_END_OF_LIST()
};

static void sm_lcd_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SSISlaveClass *k = SSI_SLAVE_CLASS(klass);

    k->init = sm_lcd_init;
    k->transfer = sm_lcd_transfer;
    k->cs_polarity = SSI_CS_LOW;
    k->parent_class.reset = sm_lcd_reset;
    dc->props = sm_lcd_properties;
}

static const TypeInfo sm_lcd_info = {
    .name          = "sm-lcd",
    .parent        = TYPE_SSI_SLAVE,
    .instance_size = sizeof(lcd_state),
    .class_init    = sm_lcd_class_init,
};

static Property sharp_mip_400_properties[] = {
    DEFINE_PROP_BOOL("rotate_display", lcd_state, rotate_display, false),
    DEFINE_PROP_UINT32("num_rows", lcd_state, num_rows, 240),
    DEFINE_PROP_UINT32("num_cols", lcd_state, num_cols, 400),
    DEFINE_PROP_END_OF_LIST()
};

static void sharp_mip_400_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SSISlaveClass *k = SSI_SLAVE_CLASS(klass);

    k->init = sm_lcd_init;
    k->transfer = sm_lcd_transfer;
    k->cs_polarity = SSI_CS_LOW;
    k->parent_class.reset = sm_lcd_reset;
    dc->props = sharp_mip_400_properties;
}

static const TypeInfo sharp_mip_400_info = {
    .name          = "sharp-mip-400x240",
    .parent        = TYPE_SSI_SLAVE,
    .instance_size = sizeof(lcd_state),
    .class_init    = sharp_mip_400_class_init,
};

#ifdef BOARD_SILK_CYBERDECK

static void sharp_mip_cyberdeck_mux_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SSISlaveClass *k = SSI_SLAVE_CLASS(klass);

    k->init = sm_lcd_init;
    k->transfer = sm_lcd_transfer;
    k->cs_polarity = SSI_CS_LOW;
    k->parent_class.reset = sm_lcd_reset;
    dc->props = sharp_mip_400_properties; /* 400x240, no rotate */
}

static const TypeInfo sharp_mip_cyberdeck_mux_info = {
    .name          = "sharp-mip-cyberdeck-mux",
    .parent        = TYPE_SSI_SLAVE,
    .instance_size = sizeof(lcd_state),
    .class_init    = sharp_mip_cyberdeck_mux_class_init,
};

/* Called from pebble_cyberdeck.c before qdev_init_nofail() */
void cyberdeck_mux_set_k230_chr(DeviceState *dev, CharDriverState *chr)
{
    lcd_state *s = FROM_SSI_SLAVE(lcd_state, SSI_SLAVE(dev));
    s->k230_chr = chr;
}

#endif /* BOARD_SILK_CYBERDECK */

static void sm_lcd_register(void)
{
    type_register_static(&sm_lcd_info);
    type_register_static(&sharp_mip_400_info);
#ifdef BOARD_SILK_CYBERDECK
    type_register_static(&sharp_mip_cyberdeck_mux_info);
#endif
}

type_init(sm_lcd_register);
