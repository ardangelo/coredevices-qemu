#include "pebble.h"
#include "hw/boards.h"
#include "hw/ssi.h"
#include "sysemu/sysemu.h"

#undef CONFIG_CURSES
#include "ui/console.h"

#include "standard-headers/linux/input.h"

#include "ui/input.h"
#include "pebble_control.h"

// Map QKeyCode (from QEMU new input API) → Linux input keycode (Pebble firmware)
// This is the same mapping as used in hw/input/virtio-input-hid.c
// KEY_* values match standard Linux KEY_* codes from <linux/input.h>
// 0 = unmapped
static const unsigned int qcode_to_pebble_key[Q_KEY_CODE_MAX] = {
    [Q_KEY_CODE_ESC]                 = KEY_ESC,
    [Q_KEY_CODE_1]                   = KEY_1,
    [Q_KEY_CODE_2]                   = KEY_2,
    [Q_KEY_CODE_3]                   = KEY_3,
    [Q_KEY_CODE_4]                   = KEY_4,
    [Q_KEY_CODE_5]                   = KEY_5,
    [Q_KEY_CODE_6]                   = KEY_6,
    [Q_KEY_CODE_7]                   = KEY_7,
    [Q_KEY_CODE_8]                   = KEY_8,
    [Q_KEY_CODE_9]                   = KEY_9,
    [Q_KEY_CODE_0]                   = KEY_0,
    [Q_KEY_CODE_MINUS]               = KEY_MINUS,
    [Q_KEY_CODE_EQUAL]               = KEY_EQUAL,
    [Q_KEY_CODE_BACKSPACE]           = KEY_BACKSPACE,

    [Q_KEY_CODE_TAB]                 = KEY_TAB,
    [Q_KEY_CODE_Q]                   = KEY_Q,
    [Q_KEY_CODE_W]                   = KEY_W,
    [Q_KEY_CODE_E]                   = KEY_E,
    [Q_KEY_CODE_R]                   = KEY_R,
    [Q_KEY_CODE_T]                   = KEY_T,
    [Q_KEY_CODE_Y]                   = KEY_Y,
    [Q_KEY_CODE_U]                   = KEY_U,
    [Q_KEY_CODE_I]                   = KEY_I,
    [Q_KEY_CODE_O]                   = KEY_O,
    [Q_KEY_CODE_P]                   = KEY_P,
    [Q_KEY_CODE_BRACKET_LEFT]        = KEY_LEFTBRACE,
    [Q_KEY_CODE_BRACKET_RIGHT]       = KEY_RIGHTBRACE,
    [Q_KEY_CODE_RET]                 = KEY_ENTER,

    [Q_KEY_CODE_CTRL]                = KEY_LEFTCTRL,
    [Q_KEY_CODE_A]                   = KEY_A,
    [Q_KEY_CODE_S]                   = KEY_S,
    [Q_KEY_CODE_D]                   = KEY_D,
    [Q_KEY_CODE_F]                   = KEY_F,
    [Q_KEY_CODE_G]                   = KEY_G,
    [Q_KEY_CODE_H]                   = KEY_H,
    [Q_KEY_CODE_J]                   = KEY_J,
    [Q_KEY_CODE_K]                   = KEY_K,
    [Q_KEY_CODE_L]                   = KEY_L,
    [Q_KEY_CODE_SEMICOLON]           = KEY_SEMICOLON,
    [Q_KEY_CODE_APOSTROPHE]          = KEY_APOSTROPHE,
    [Q_KEY_CODE_GRAVE_ACCENT]        = KEY_GRAVE,

    [Q_KEY_CODE_SHIFT]               = KEY_LEFTSHIFT,
    [Q_KEY_CODE_BACKSLASH]           = KEY_BACKSLASH,
    [Q_KEY_CODE_Z]                   = KEY_Z,
    [Q_KEY_CODE_X]                   = KEY_X,
    [Q_KEY_CODE_C]                   = KEY_C,
    [Q_KEY_CODE_V]                   = KEY_V,
    [Q_KEY_CODE_B]                   = KEY_B,
    [Q_KEY_CODE_N]                   = KEY_N,
    [Q_KEY_CODE_M]                   = KEY_M,
    [Q_KEY_CODE_COMMA]               = KEY_COMMA,
    [Q_KEY_CODE_DOT]                 = KEY_DOT,
    [Q_KEY_CODE_SLASH]               = KEY_SLASH,
    [Q_KEY_CODE_SHIFT_R]             = KEY_RIGHTSHIFT,

    [Q_KEY_CODE_ALT]                 = KEY_LEFTALT,
    [Q_KEY_CODE_SPC]                 = KEY_SPACE,
    [Q_KEY_CODE_CAPS_LOCK]           = KEY_CAPSLOCK,

    [Q_KEY_CODE_F1]                  = KEY_F1,
    [Q_KEY_CODE_F2]                  = KEY_F2,
    [Q_KEY_CODE_F3]                  = KEY_F3,
    [Q_KEY_CODE_F4]                  = KEY_F4,
    [Q_KEY_CODE_F5]                  = KEY_F5,
    [Q_KEY_CODE_F6]                  = KEY_F6,
    [Q_KEY_CODE_F7]                  = KEY_F7,
    [Q_KEY_CODE_F8]                  = KEY_F8,
    [Q_KEY_CODE_F9]                  = KEY_F9,
    [Q_KEY_CODE_F10]                 = KEY_F10,
    [Q_KEY_CODE_NUM_LOCK]            = KEY_NUMLOCK,
    [Q_KEY_CODE_SCROLL_LOCK]         = KEY_SCROLLLOCK,

    [Q_KEY_CODE_KP_0]                = KEY_KP0,
    [Q_KEY_CODE_KP_1]                = KEY_KP1,
    [Q_KEY_CODE_KP_2]                = KEY_KP2,
    [Q_KEY_CODE_KP_3]                = KEY_KP3,
    [Q_KEY_CODE_KP_4]                = KEY_KP4,
    [Q_KEY_CODE_KP_5]                = KEY_KP5,
    [Q_KEY_CODE_KP_6]                = KEY_KP6,
    [Q_KEY_CODE_KP_7]                = KEY_KP7,
    [Q_KEY_CODE_KP_8]                = KEY_KP8,
    [Q_KEY_CODE_KP_9]                = KEY_KP9,
    [Q_KEY_CODE_KP_SUBTRACT]         = KEY_KPMINUS,
    [Q_KEY_CODE_KP_ADD]              = KEY_KPPLUS,
    [Q_KEY_CODE_KP_DECIMAL]          = KEY_KPDOT,
    [Q_KEY_CODE_KP_ENTER]            = KEY_KPENTER,
    [Q_KEY_CODE_KP_DIVIDE]           = KEY_KPSLASH,
    [Q_KEY_CODE_KP_MULTIPLY]         = KEY_KPASTERISK,

    [Q_KEY_CODE_F11]                 = KEY_F11,
    [Q_KEY_CODE_F12]                 = KEY_F12,

    [Q_KEY_CODE_CTRL_R]              = KEY_RIGHTCTRL,
    [Q_KEY_CODE_SYSRQ]               = KEY_SYSRQ,
    [Q_KEY_CODE_ALT_R]               = KEY_RIGHTALT,

    [Q_KEY_CODE_HOME]                = KEY_HOME,
    [Q_KEY_CODE_UP]                  = KEY_UP,
    [Q_KEY_CODE_PGUP]                = KEY_PAGEUP,
    [Q_KEY_CODE_LEFT]                = KEY_LEFT,
    [Q_KEY_CODE_RIGHT]               = KEY_RIGHT,
    [Q_KEY_CODE_END]                 = KEY_END,
    [Q_KEY_CODE_DOWN]                = KEY_DOWN,
    [Q_KEY_CODE_PGDN]                = KEY_PAGEDOWN,
    [Q_KEY_CODE_INSERT]              = KEY_INSERT,
    [Q_KEY_CODE_DELETE]              = KEY_DELETE,

    [Q_KEY_CODE_META_L]              = KEY_LEFTMETA,
    [Q_KEY_CODE_META_R]              = KEY_RIGHTMETA,
    [Q_KEY_CODE_MENU]                = KEY_MENU,
};

typedef struct {
    PebbleControl *pctrl;
    uint32_t button_state;
} CyberdeckKbdState;

static void cyberdeck_kbd_event(DeviceState *dev, QemuConsole *src,
                                InputEvent *evt)
{
    CyberdeckKbdState *s = (CyberdeckKbdState *)dev;

    if (evt->type != INPUT_EVENT_KIND_KEY) {
        return;
    }

    // Only process keyboard events when the QEMU window is focused
    if (src && !qemu_console_is_visible(src)) {
        return;
    }

    InputKeyEvent *key = evt->u.key;
    bool is_down = key->down;

    // Workaround to fix wrong arrow key scancodes
    if (key->key->type == KEY_VALUE_KIND_NUMBER) {
        int64_t number = key->key->u.number;
        switch (number) {
            case 0xb7: key->key->u.number = 0xc8; break;
            case 0xb8: key->key->u.number = 0xcb; break;
            case 0xc6: key->key->u.number = 0xcd; break;
            case 0x0:  key->key->u.number = 0xd0; break;
        }
    }

    // Debug: Print KeyValue details BEFORE conversion
    fprintf(stderr, "cyberdeck_kbd: KeyValue type=%d ", key->key->type);
    if (key->key->type == KEY_VALUE_KIND_NUMBER) {
        fprintf(stderr, "number=0x%llx ", (unsigned long long)key->key->u.number);
    } else if (key->key->type == KEY_VALUE_KIND_QCODE) {
        fprintf(stderr, "qcode=%d ", key->key->u.qcode);
    }
    fprintf(stderr, "down=%d\n", is_down);

    // Now convert to qcode
    int qcode = qemu_input_key_value_to_qcode(key->key);
    fprintf(stderr, "  -> converted qcode=0x%x (%d)\n", qcode, qcode);

    /* Arrow keys → nav buttons (for Pebble compatibility) */
    PblButtonID button_id = PBL_BUTTON_ID_NONE;
    switch (qcode) {
        case Q_KEY_CODE_UP:
            button_id = PBL_BUTTON_ID_UP;
            fprintf(stderr, "  Mapping to PBL_BUTTON_ID_UP\n");
            break;
        case Q_KEY_CODE_DOWN:
            button_id = PBL_BUTTON_ID_DOWN;
            fprintf(stderr, "  Mapping to PBL_BUTTON_ID_DOWN\n");
            break;
        case Q_KEY_CODE_LEFT:
            button_id = PBL_BUTTON_ID_BACK;
            fprintf(stderr, "  Mapping to PBL_BUTTON_ID_BACK\n");
            break;
        case Q_KEY_CODE_RIGHT:
            button_id = PBL_BUTTON_ID_SELECT;
            fprintf(stderr, "  Mapping to PBL_BUTTON_ID_SELECT\n");
            break;
    }

    if (button_id != PBL_BUTTON_ID_NONE) {
        if (is_down) {
            s->button_state |= (1 << button_id);
        } else {
            s->button_state &= ~(1 << button_id);
        }
        pebble_set_button_state(s->button_state);
        return;
    }

    /* All other keys - keyboard events to firmware */
    if (qcode >= 0 && qcode < Q_KEY_CODE_MAX) {
        unsigned int pkey = qcode_to_pebble_key[qcode];
        if (pkey != 0) {
            if (s->pctrl) {
                fprintf(stderr, "  Sending keyboard event: pkey=%d\n", pkey);
                pebble_control_send_keyboard_event(s->pctrl, pkey, is_down);
            } else {
                fprintf(stderr, "  ERROR: pctrl is NULL!\n");
            }
        } else {
            if (is_down) {
                fprintf(stderr, "  Unmapped qcode: %d\n", qcode);
            }
        }
    }
}

static QemuInputHandler cyberdeck_kbd_handler = {
    .name  = "cyberdeck-keyboard",
    .mask  = INPUT_EVENT_MASK_KEY,
    .event = cyberdeck_kbd_event,
};

static CyberdeckKbdState s_cyberdeck_kbd;

static const PblBoardConfig s_board_config_cyberdeck = {
    .dbgserial_uart_index = 0,       // USART1 -> debug serial
    .pebble_control_uart_index = 1,  // USART2 -> pebble control
    .button_map = {
        // Arrow keys for nav buttons (alpha keys reserved for keyboard)
        // Adjust GPIO indices to match Silk if needed, or stick to this map if it works with QEMU keymaps
        { STM32_GPIOC_INDEX, 13, true },  // BACK  <- Left arrow
        { STM32_GPIOD_INDEX, 2, true },   // UP    <- Up arrow
        { STM32_GPIOH_INDEX, 0, true },   // SELECT <- Right arrow
        { STM32_GPIOH_INDEX, 1, true },   // DOWN  <- Down arrow
    },
    .gpio_idr_masks = {
      [STM32_GPIOC_INDEX] = 1 << 13,
      [STM32_GPIOD_INDEX] = 1 << 2,
      [STM32_GPIOH_INDEX] = (1 << 1) | (1 << 0),
    },
    .flash_size = 4096,  // 4MB for QEMU dev
    .ram_size = 256,
    .num_rows = 240,
    .num_cols = 400,
    .num_border_rows = 0,
    .num_border_cols = 0,
    .row_major = false,
    .row_inverted = false,
    .col_inverted = false,
    .round_mask = false
};

static void pebble_cyberdeck_init(MachineState *machine)
{
    Stm32Gpio *gpio[STM32F4XX_GPIO_COUNT];
    Stm32Uart *uart[STM32F4XX_UART_COUNT];
    Stm32Timer *timer[STM32F4XX_TIM_COUNT];
    DeviceState *qspi_flash;
    DeviceState *rtc_dev;
    SSIBus *spi;
    SSIBus *qspi;
    struct stm32f4xx stm;
    ARMCPU *cpu;

    const PblBoardConfig *board_config = &s_board_config_cyberdeck;

    // Note: allow for bigger flash images (4MByte) to aid in development and debugging
    stm32f4xx_init(board_config->flash_size,
                   board_config->ram_size,
                   machine->kernel_filename,
                   gpio,
                   board_config->gpio_idr_masks,
                   uart,
                   timer,
                   &rtc_dev,
                   8000000 /*osc_freq*/,
                   32768 /*osc2_freq*/,
                   &stm,
                   &cpu);


    // Set the Pebble specific QEMU settings on the target
    pebble_set_qemu_settings(rtc_dev);

    /* --- QSPI Flash ---------------------------------------------  */
    qspi = (SSIBus *)qdev_get_child_bus(stm.qspi_dev, "ssi");
    qspi_flash = ssi_create_slave_no_init(qspi, "mx25u6435f");
    qdev_init_nofail(qspi_flash);

    qemu_irq mx25u_cs = qdev_get_gpio_in_named(qspi_flash, SSI_GPIO_CS, 0);
    qdev_connect_gpio_out_named(stm.qspi_dev, "qspi-gpio-cs", 0, mx25u_cs);


    /* --- Display ------------------------------------------------  */
    // Use the 400x240 Sharp MIP display
    spi = (SSIBus *)qdev_get_child_bus(stm.spi_dev[1], "ssi"); // SPI2
    DeviceState *display_dev = ssi_create_slave_no_init(spi, "sharp-mip-400x240");
    // Dimensions etc are set by default property values for this type
    qdev_init_nofail(display_dev);

    qemu_irq backlight_enable;
    backlight_enable = qdev_get_gpio_in_named(display_dev, "backlight_enable", 0);
    qdev_connect_gpio_out((DeviceState *)gpio[STM32_GPIOB_INDEX], 13, backlight_enable);

    qemu_irq backlight_level;
    backlight_level = qdev_get_gpio_in_named(display_dev, "backlight_level", 0);
    qdev_connect_gpio_out_named((DeviceState *)timer[2], // TIM3
                                "pwm_ratio_changed",
                                0,
                                backlight_level);

    qemu_irq display_power;
    display_power = qdev_get_gpio_in_named(display_dev, "power_ctl", 0);
    qdev_connect_gpio_out_named((DeviceState *)cpu->env.nvic, "power_out", 0,
                                  display_power);


    // Connect up the uarts
    PebbleControl *pctrl = pebble_connect_uarts(uart, board_config);
    
    // Wire K230 UART (USART3) to serial_hds[3]
    if (serial_hds[3]) {
        stm32_uart_connect(uart[2], serial_hds[3], 0);
    }


    // Init the buttons IRQs (without default handler)
    pebble_init_button_irqs(gpio, board_config->button_map);
    
    // Register keyboard/button handler using the new QEMU input API
    // (bypasses broken scancode translation in the legacy path)
    s_cyberdeck_kbd.pctrl = pctrl;
    s_cyberdeck_kbd.button_state = 0;
    QemuInputHandlerState *ihs = qemu_input_handler_register(
        (DeviceState *)&s_cyberdeck_kbd, &cyberdeck_kbd_handler);
    qemu_input_handler_activate(ihs);

    // Create the board device and wire it up
    qemu_irq display_vibe;
    display_vibe = qdev_get_gpio_in_named(display_dev, "vibe_ctl", 0);
    DeviceState *board = pebble_init_board(gpio, display_vibe);

    // The vibe PWM enables the qemu vibe
    qemu_irq board_vibe_in;
    board_vibe_in = qdev_get_gpio_in_named(board, "pebble_board_vibe_in", 0);
    qdev_connect_gpio_out_named((DeviceState *)timer[13], // TIM14
                                "pwm_enable",
                                0,
                                board_vibe_in);
}

static void pebble_cyberdeck_machine_init(MachineClass *mc)
{
    mc->desc = "Pebble Cyberdeck";
    mc->init = pebble_cyberdeck_init;
}

DEFINE_MACHINE("pebble-cyberdeck", pebble_cyberdeck_machine_init)
