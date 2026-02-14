#include "pebble.h"
#include "hw/boards.h"
#include "hw/ssi.h"
#include "sysemu/sysemu.h"
#include "ui/console.h"
#include "pebble_control.h"

// Pebble Key IDs from src/fw/services/common/keyboard_codes.h
#define PBL_KEY_ESC 1
#define PBL_KEY_1 2
#define PBL_KEY_2 3
#define PBL_KEY_3 4
#define PBL_KEY_4 5
#define PBL_KEY_5 6
#define PBL_KEY_6 7
#define PBL_KEY_7 8
#define PBL_KEY_8 9
#define PBL_KEY_9 10
#define PBL_KEY_0 11
#define PBL_KEY_MINUS 12
#define PBL_KEY_EQUAL 13
#define PBL_KEY_BACKSPACE 14
#define PBL_KEY_TAB 15
#define PBL_KEY_Q 16
#define PBL_KEY_W 17
#define PBL_KEY_E 18
#define PBL_KEY_R 19
#define PBL_KEY_T 20
#define PBL_KEY_Y 21
#define PBL_KEY_U 22
#define PBL_KEY_I 23
#define PBL_KEY_O 24
#define PBL_KEY_P 25
#define PBL_KEY_LEFTBRACE 26
#define PBL_KEY_RIGHTBRACE 27
#define PBL_KEY_ENTER 28
#define PBL_KEY_LEFTCTRL 29
#define PBL_KEY_A 30
#define PBL_KEY_S 31
#define PBL_KEY_D 32
#define PBL_KEY_F 33
#define PBL_KEY_G 34
#define PBL_KEY_H 35
#define PBL_KEY_J 36
#define PBL_KEY_K 37
#define PBL_KEY_L 38
#define PBL_KEY_SEMICOLON 39
#define PBL_KEY_APOSTROPHE 40
#define PBL_KEY_GRAVE 41
#define PBL_KEY_LEFTSHIFT 42
#define PBL_KEY_BACKSLASH 43
#define PBL_KEY_Z 44
#define PBL_KEY_X 45
#define PBL_KEY_C 46
#define PBL_KEY_V 47
#define PBL_KEY_B 48
#define PBL_KEY_N 49
#define PBL_KEY_M 50
#define PBL_KEY_COMMA 51
#define PBL_KEY_DOT 52
#define PBL_KEY_SLASH 53
#define PBL_KEY_RIGHTSHIFT 54
#define PBL_KEY_KPASTERISK 55
#define PBL_KEY_LEFTALT 56
#define PBL_KEY_SPACE 57
#define PBL_KEY_CAPSLOCK 58
#define PBL_KEY_F1 59
#define PBL_KEY_F2 60
#define PBL_KEY_F3 61
#define PBL_KEY_F4 62
#define PBL_KEY_F5 63
#define PBL_KEY_F6 64
#define PBL_KEY_F7 65
#define PBL_KEY_F8 66
#define PBL_KEY_F9 67
#define PBL_KEY_F10 68
#define PBL_KEY_NUMLOCK 69
#define PBL_KEY_SCROLLLOCK 70
#define PBL_KEY_KP7 71
#define PBL_KEY_KP8 72
#define PBL_KEY_KP9 73
#define PBL_KEY_KPMINUS 74
#define PBL_KEY_KP4 75
#define PBL_KEY_KP5 76
#define PBL_KEY_KP6 77
#define PBL_KEY_KPPLUS 78
#define PBL_KEY_KP1 79
#define PBL_KEY_KP2 80
#define PBL_KEY_KP3 81
#define PBL_KEY_KP0 82
#define PBL_KEY_KPDOT 83
#define PBL_KEY_F11 87
#define PBL_KEY_F12 88
#define PBL_KEY_KPENTER 96
#define PBL_KEY_RIGHTCTRL 97
#define PBL_KEY_KPSLASH 98
#define PBL_KEY_SYSRQ 99
#define PBL_KEY_RIGHTALT 100
#define PBL_KEY_HOME 102
#define PBL_KEY_UP 103
#define PBL_KEY_PAGEUP 104
#define PBL_KEY_LEFT 105
#define PBL_KEY_RIGHT 106
#define PBL_KEY_END 107
#define PBL_KEY_DOWN 108
#define PBL_KEY_PAGEDOWN 109
#define PBL_KEY_INSERT 110
#define PBL_KEY_DELETE 111

static uint8_t scancode_to_pebble_key(int code) {
    switch (code) {
        case 0x01: return PBL_KEY_ESC;
        case 0x02: return PBL_KEY_1;
        case 0x03: return PBL_KEY_2;
        case 0x04: return PBL_KEY_3;
        case 0x05: return PBL_KEY_4;
        case 0x06: return PBL_KEY_5;
        case 0x07: return PBL_KEY_6;
        case 0x08: return PBL_KEY_7;
        case 0x09: return PBL_KEY_8;
        case 0x0a: return PBL_KEY_9;
        case 0x0b: return PBL_KEY_0;
        case 0x0c: return PBL_KEY_MINUS;
        case 0x0d: return PBL_KEY_EQUAL;
        case 0x0e: return PBL_KEY_BACKSPACE;
        case 0x0f: return PBL_KEY_TAB;
        case 0x10: return PBL_KEY_Q;
        case 0x11: return PBL_KEY_W;
        case 0x12: return PBL_KEY_E;
        case 0x13: return PBL_KEY_R;
        case 0x14: return PBL_KEY_T;
        case 0x15: return PBL_KEY_Y;
        case 0x16: return PBL_KEY_U;
        case 0x17: return PBL_KEY_I;
        case 0x18: return PBL_KEY_O;
        case 0x19: return PBL_KEY_P;
        case 0x1a: return PBL_KEY_LEFTBRACE;
        case 0x1b: return PBL_KEY_RIGHTBRACE;
        case 0x1c: return PBL_KEY_ENTER;
        case 0x1d: return PBL_KEY_LEFTCTRL;
        case 0x1e: return PBL_KEY_A;
        case 0x1f: return PBL_KEY_S;
        case 0x20: return PBL_KEY_D;
        case 0x21: return PBL_KEY_F;
        case 0x22: return PBL_KEY_G;
        case 0x23: return PBL_KEY_H;
        case 0x24: return PBL_KEY_J;
        case 0x25: return PBL_KEY_K;
        case 0x26: return PBL_KEY_L;
        case 0x27: return PBL_KEY_SEMICOLON;
        case 0x28: return PBL_KEY_APOSTROPHE;
        case 0x29: return PBL_KEY_GRAVE;
        case 0x2a: return PBL_KEY_LEFTSHIFT;
        case 0x2b: return PBL_KEY_BACKSLASH;
        case 0x2c: return PBL_KEY_Z;
        case 0x2d: return PBL_KEY_X;
        case 0x2e: return PBL_KEY_C;
        case 0x2f: return PBL_KEY_V;
        case 0x30: return PBL_KEY_B;
        case 0x31: return PBL_KEY_N;
        case 0x32: return PBL_KEY_M;
        case 0x33: return PBL_KEY_COMMA;
        case 0x34: return PBL_KEY_DOT;
        case 0x35: return PBL_KEY_SLASH;
        case 0x36: return PBL_KEY_RIGHTSHIFT;
        case 0x37: return PBL_KEY_KPASTERISK;
        case 0x38: return PBL_KEY_LEFTALT;
        case 0x39: return PBL_KEY_SPACE;
        case 0x3a: return PBL_KEY_CAPSLOCK;
        case 0x3b: return PBL_KEY_F1;
        case 0x3c: return PBL_KEY_F2;
        case 0x3d: return PBL_KEY_F3;
        case 0x3e: return PBL_KEY_F4;
        case 0x3f: return PBL_KEY_F5;
        case 0x40: return PBL_KEY_F6;
        case 0x41: return PBL_KEY_F7;
        case 0x42: return PBL_KEY_F8;
        case 0x43: return PBL_KEY_F9;
        case 0x44: return PBL_KEY_F10;
        case 0x45: return PBL_KEY_NUMLOCK;
        case 0x46: return PBL_KEY_SCROLLLOCK;
        case 0x47: return PBL_KEY_KP7;
        case 0x48: return PBL_KEY_KP8;
        case 0x49: return PBL_KEY_KP9;
        case 0x4a: return PBL_KEY_KPMINUS;
        case 0x4b: return PBL_KEY_KP4;
        case 0x4c: return PBL_KEY_KP5;
        case 0x4d: return PBL_KEY_KP6;
        case 0x4e: return PBL_KEY_KPPLUS;
        case 0x4f: return PBL_KEY_KP1;
        case 0x50: return PBL_KEY_KP2;
        case 0x51: return PBL_KEY_KP3;
        case 0x52: return PBL_KEY_KP0;
        case 0x53: return PBL_KEY_KPDOT;
        case 0x57: return PBL_KEY_F11;
        case 0x58: return PBL_KEY_F12;
        case 0x9c: return PBL_KEY_KPENTER;
        case 0x9d: return PBL_KEY_RIGHTCTRL;
        case 0xb8: return PBL_KEY_RIGHTALT;
        case 0xc7: return PBL_KEY_HOME;
        case 0xc8: return PBL_KEY_UP;
        case 0xc9: return PBL_KEY_PAGEUP;
        case 0xcb: return PBL_KEY_LEFT;
        case 0xcd: return PBL_KEY_RIGHT;
        case 0xcf: return PBL_KEY_END;
        case 0xd0: return PBL_KEY_DOWN;
        case 0xd1: return PBL_KEY_PAGEDOWN;
        case 0xd2: return PBL_KEY_INSERT;
        case 0xd3: return PBL_KEY_DELETE;
    }
    return 0;
}

static void pebble_cyberdeck_key_handler(void *opaque, int keycode)
{
    PebbleControl *s = opaque;
    bool is_down = !(keycode & 0x80);
    int code = keycode & 0x7f;
    
    // Check for arrow keys and map to nav buttons 
    static uint32_t s_button_state = 0;
    
    int button_id = PBL_BUTTON_ID_NONE;
    static int prev_keycode = 0;
    
    if (code == 224) { // 0xE0
        prev_keycode = keycode;
        return;
    }

    // Arrow keys (standard set 1)
    // Note: If extended (prev_keycode was 0xE0), we can map them.
    // QEMU 0x75=Left, 0x72=Down etc.. waiting, above table says 75 is KEY_KP4.
    // Wait, Standard Set 1:
    // UP: E0 48
    // DOWN: E0 50
    // LEFT: E0 4B
    // RIGHT: E0 4D
    // My table says:
    // 0x48 = KP8 (Up arrow on numpad) -> KEY_KP8
    // 0x4B = KP4 (Left arrow on numpad) -> KEY_KP4
    // 0x50 = KP2 (Down arrow on numpad) -> KEY_KP2
    // 0x4D = KP6 (Right arrow on numpad) -> KEY_KP6
    
    // Extended keys map to dedicated arrows:
    // E0 48 -> UP
    
    // pebble.c pebble_key_handler uses raw codes: 72, 80, 75, 77.
    // 72 (0x48) = Up
    // 80 (0x50) = Down
    // 75 (0x4B) = Left
    // 77 (0x4D) = Right
    // And checks prev_keycode == 224.
    
    if (prev_keycode == 224 || prev_keycode == (224 | 0x80)) {
        switch (code) {
            case 72: button_id = PBL_BUTTON_ID_UP; break; // Up
            case 80: button_id = PBL_BUTTON_ID_DOWN; break; // Down
            case 75: button_id = PBL_BUTTON_ID_BACK; break; // Left -> Back
            case 77: button_id = PBL_BUTTON_ID_SELECT; break; // Right -> Select
        }
    }
    
    prev_keycode = keycode;
    
    if (button_id != PBL_BUTTON_ID_NONE) {
        if (is_down) {
            s_button_state |= (1 << button_id);
        } else {
            s_button_state &= ~(1 << button_id);
        }
        pebble_set_button_state(s_button_state);
        // Do NOT consume arrow keys for keyboard input if they are nav buttons
        return;
    }
    
    // Map to Pebble Key ID
    uint8_t pkey = scancode_to_pebble_key(code);
    if (pkey != 0) {
        if (s) {
            pebble_control_send_keyboard_event(s, pkey, is_down);
        }
    }
}

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
    
    // Register our custom keyboard/button handler
    qemu_add_kbd_event_handler(pebble_cyberdeck_key_handler, pctrl);

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
