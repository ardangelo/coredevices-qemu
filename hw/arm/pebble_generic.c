/*
 * Pebble Generic Machine Types
 *
 * Generic ARMv7M-based machine types for new Pebble platforms.
 * Uses simple custom MMIO peripherals instead of MCU-specific emulation.
 *
 * Machine types:
 *   pebble-emery   - Cortex-M33, 512KB RAM, 4MB flash
 *   pebble-flint   - Cortex-M4, 256KB RAM, 4MB flash
 *   pebble-gabbro  - Cortex-M33, 512KB RAM, 4MB flash
 *   pebble-cyberdeck-evt3 - Cortex-M4, 512KB RAM, 4MB flash
 *
 * Copyright (c) 2026 Core Devices LLC
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/qdev-clock.h"
#include "hw/sysbus.h"
#include "hw/misc/unimp.h"
#include "chardev/char-fe.h"
#include "system/address-spaces.h"
#include "system/system.h"
#include "audio/audio.h"
#include "system/blockdev.h"
#include "exec/cpu-common.h"
#include "qom/object.h"
#include "hw/arm/pebble_generic.h"
#include "hw/misc/pebble_null_input.h"
#include "pebble_control.h"
#include "hw/arm/pebble_gpio.h"
#include "standard-headers/linux/input.h"
#include "ui/input.h"
#include "ui/console.h"

/* ===== Board configurations ===== */

static const PblGenericBoardConfig board_cfg_emery = {
    .name          = "pebble-emery",
    .desc          = "Pebble Emery (obelix, Cortex-M33)",
    .cpu_type      = ARM_CPU_TYPE_NAME("cortex-m33"),
    .board_type    = PBL_BOARD_EMERY,
    .board_id      = PBL_BOARD_ID_EMERY,
    .flash_size    = 4 * MiB,
    .ram_size      = 512 * KiB,
    .sysclk_frq    = PBL_SYSCLK_FRQ,
    .display_width = 200,
    .display_height = 228,
    .display_bpp   = 8,
    .display_round = false,
    .has_touch     = true,
    .has_audio     = true,
};

static const PblGenericBoardConfig board_cfg_flint = {
    .name          = "pebble-flint",
    .desc          = "Pebble Flint (asterix, Cortex-M4)",
    .cpu_type      = ARM_CPU_TYPE_NAME("cortex-m4"),
    .board_type    = PBL_BOARD_FLINT,
    .board_id      = PBL_BOARD_ID_FLINT,
    .flash_size    = 4 * MiB,
    .ram_size      = 256 * KiB,
    .sysclk_frq    = PBL_SYSCLK_FRQ,
    .display_width = 144,
    .display_height = 168,
    .display_bpp   = 1,
    .display_round = false,
    .has_touch     = false,
    .has_audio     = true,
};

static const PblGenericBoardConfig board_cfg_cyberdeck_evt3 = {
    .name          = "pebble-cyberdeck-evt3",
    .desc          = "Pebble Cyberdeck EVT3 (Cortex-M4)",
    .cpu_type      = ARM_CPU_TYPE_NAME("cortex-m4"),
    .board_type    = PBL_BOARD_CYBERDECK_EVT3,
    .board_id      = PBL_BOARD_ID_CYBERDECK_EVT3,
    .flash_size    = 4 * MiB,
    .ram_size      = 512 * KiB,
    .sysclk_frq    = PBL_SYSCLK_FRQ,
    .display_width = 400,
    .display_height = 240,
    .display_bpp   = 1,
    .display_round = false,
    .has_touch     = false,
    .has_audio     = true,
};

static const PblGenericBoardConfig board_cfg_gabbro = {
    .name          = "pebble-gabbro",
    .desc          = "Pebble Gabbro (getafix, Cortex-M33)",
    .cpu_type      = ARM_CPU_TYPE_NAME("cortex-m33"),
    .board_type    = PBL_BOARD_GABBRO,
    .board_id      = PBL_BOARD_ID_GABBRO,
    .flash_size    = 4 * MiB,
    .ram_size      = 512 * KiB,
    .sysclk_frq    = PBL_SYSCLK_FRQ,
    .display_width = 260,
    .display_height = 260,
    .display_bpp   = 8,
    .display_round = true,
    .has_touch     = true,
    .has_audio     = false,
};

typedef struct {
    PebbleControl *pctrl;
    uint32_t button_state;
} CyberdeckKbdState;

static const unsigned int qcode_to_cyberdeck_key[Q_KEY_CODE__MAX] = {
    [Q_KEY_CODE_ESC] = KEY_ESC,
    [Q_KEY_CODE_1] = KEY_1,
    [Q_KEY_CODE_2] = KEY_2,
    [Q_KEY_CODE_3] = KEY_3,
    [Q_KEY_CODE_4] = KEY_4,
    [Q_KEY_CODE_5] = KEY_5,
    [Q_KEY_CODE_6] = KEY_6,
    [Q_KEY_CODE_7] = KEY_7,
    [Q_KEY_CODE_8] = KEY_8,
    [Q_KEY_CODE_9] = KEY_9,
    [Q_KEY_CODE_0] = KEY_0,
    [Q_KEY_CODE_MINUS] = KEY_MINUS,
    [Q_KEY_CODE_EQUAL] = KEY_EQUAL,
    [Q_KEY_CODE_BACKSPACE] = KEY_BACKSPACE,
    [Q_KEY_CODE_TAB] = KEY_TAB,
    [Q_KEY_CODE_Q] = KEY_Q,
    [Q_KEY_CODE_W] = KEY_W,
    [Q_KEY_CODE_E] = KEY_E,
    [Q_KEY_CODE_R] = KEY_R,
    [Q_KEY_CODE_T] = KEY_T,
    [Q_KEY_CODE_Y] = KEY_Y,
    [Q_KEY_CODE_U] = KEY_U,
    [Q_KEY_CODE_I] = KEY_I,
    [Q_KEY_CODE_O] = KEY_O,
    [Q_KEY_CODE_P] = KEY_P,
    [Q_KEY_CODE_BRACKET_LEFT] = KEY_LEFTBRACE,
    [Q_KEY_CODE_BRACKET_RIGHT] = KEY_RIGHTBRACE,
    [Q_KEY_CODE_RET] = KEY_ENTER,
    [Q_KEY_CODE_CTRL] = KEY_LEFTCTRL,
    [Q_KEY_CODE_A] = KEY_A,
    [Q_KEY_CODE_S] = KEY_S,
    [Q_KEY_CODE_D] = KEY_D,
    [Q_KEY_CODE_F] = KEY_F,
    [Q_KEY_CODE_G] = KEY_G,
    [Q_KEY_CODE_H] = KEY_H,
    [Q_KEY_CODE_J] = KEY_J,
    [Q_KEY_CODE_K] = KEY_K,
    [Q_KEY_CODE_L] = KEY_L,
    [Q_KEY_CODE_SEMICOLON] = KEY_SEMICOLON,
    [Q_KEY_CODE_APOSTROPHE] = KEY_APOSTROPHE,
    [Q_KEY_CODE_GRAVE_ACCENT] = KEY_GRAVE,
    [Q_KEY_CODE_SHIFT] = KEY_LEFTSHIFT,
    [Q_KEY_CODE_BACKSLASH] = KEY_BACKSLASH,
    [Q_KEY_CODE_Z] = KEY_Z,
    [Q_KEY_CODE_X] = KEY_X,
    [Q_KEY_CODE_C] = KEY_C,
    [Q_KEY_CODE_V] = KEY_V,
    [Q_KEY_CODE_B] = KEY_B,
    [Q_KEY_CODE_N] = KEY_N,
    [Q_KEY_CODE_M] = KEY_M,
    [Q_KEY_CODE_COMMA] = KEY_COMMA,
    [Q_KEY_CODE_DOT] = KEY_DOT,
    [Q_KEY_CODE_SLASH] = KEY_SLASH,
    [Q_KEY_CODE_SHIFT_R] = KEY_RIGHTSHIFT,
    [Q_KEY_CODE_ALT] = KEY_LEFTALT,
    [Q_KEY_CODE_SPC] = KEY_SPACE,
    [Q_KEY_CODE_CAPS_LOCK] = KEY_CAPSLOCK,
    [Q_KEY_CODE_F1] = KEY_F1,
    [Q_KEY_CODE_F2] = KEY_F2,
    [Q_KEY_CODE_F3] = KEY_F3,
    [Q_KEY_CODE_F4] = KEY_F4,
    [Q_KEY_CODE_F5] = KEY_F5,
    [Q_KEY_CODE_F6] = KEY_F6,
    [Q_KEY_CODE_F7] = KEY_F7,
    [Q_KEY_CODE_F8] = KEY_F8,
    [Q_KEY_CODE_F9] = KEY_F9,
    [Q_KEY_CODE_F10] = KEY_F10,
};

static void cyberdeck_kbd_event(DeviceState *dev, QemuConsole *src, InputEvent *evt)
{
    CyberdeckKbdState *state = (CyberdeckKbdState *)dev;

    if (evt->type != INPUT_EVENT_KIND_KEY) {
        return;
    }
    (void)src;

    InputKeyEvent *key = evt->u.key.data;
    int qcode = qemu_input_key_value_to_qcode(key->key);
    bool is_down = key->down;

    uint32_t button_mask = 0;
    switch (qcode) {
    case Q_KEY_CODE_LEFT:
        button_mask = PBL_BTN_BACK;
        break;
    case Q_KEY_CODE_UP:
        button_mask = PBL_BTN_UP;
        break;
    case Q_KEY_CODE_RIGHT:
        button_mask = PBL_BTN_SELECT;
        break;
    case Q_KEY_CODE_DOWN:
        button_mask = PBL_BTN_DOWN;
        break;
    }
    if (button_mask) {
        if (is_down) {
            state->button_state |= button_mask;
        } else {
            state->button_state &= ~button_mask;
        }
        pbl_gpio_set_button_state(state->button_state);
        return;
    }

    if (qcode >= 0 && qcode < Q_KEY_CODE__MAX && state->pctrl) {
        unsigned int keycode = qcode_to_cyberdeck_key[qcode];
        if (keycode) {
            pebble_control_send_keyboard_event(state->pctrl, keycode, is_down);
        }
    }
}

static QemuInputHandler cyberdeck_kbd_handler = {
    .name = "cyberdeck-keyboard",
    .mask = INPUT_EVENT_MASK_KEY,
    .event = cyberdeck_kbd_event,
};

static CyberdeckKbdState s_cyberdeck_kbd;

/* ===== Machine init ===== */

static void pbl_generic_init(MachineState *machine)
{
    PblGenericMachineState *s = PBL_GENERIC_MACHINE(machine);
    PblGenericMachineClass *mc = PBL_GENERIC_MACHINE_GET_CLASS(machine);
    const PblGenericBoardConfig *cfg = mc->board_cfg;
    MemoryRegion *system_memory = get_system_memory();
    DeviceState *armv7m;

    /* Clocks */
    s->sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(s->sysclk, cfg->sysclk_frq);

    s->refclk = clock_new(OBJECT(machine), "REFCLK");
    clock_set_hz(s->refclk, PBL_REFCLK_FRQ);

    /* Flash (code memory) */
    memory_region_init_ram(&s->flash, NULL, "pebble.flash",
                           cfg->flash_size, &error_fatal);
    memory_region_set_readonly(&s->flash, true);
    memory_region_add_subregion(system_memory, PBL_FLASH_BASE, &s->flash);

    /* SRAM */
    memory_region_init_ram(&s->sram, NULL, "pebble.sram",
                           cfg->ram_size, &error_fatal);
    memory_region_add_subregion(system_memory, PBL_SRAM_BASE, &s->sram);

    /* ARMv7M CPU + NVIC */
    object_initialize_child(OBJECT(s), "armv7m", &s->armv7m, TYPE_ARMV7M);
    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", PBL_NUM_IRQS);
    /* Pebble firmware assumes 3 priority bits (__NVIC_PRIO_BITS=3), matching
     * the nRF52/STM32F4 watches. QEMU defaults to 8 for ARMv7+, which means
     * CMSIS-shifted IRQ priorities like 0xA0 are numerically less than
     * BASEPRI=0xBF and would preempt critical sections — leaking
     * uxCriticalNesting via portCLEAR_INTERRUPT_MASK_FROM_ISR.
     */
    qdev_prop_set_uint8(armv7m, "num-prio-bits", 3);
    qdev_prop_set_string(armv7m, "cpu-type", cfg->cpu_type);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_connect_clock_in(armv7m, "refclk", s->refclk);
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), &error_fatal);

    /* === UARTs === */
    DeviceState *uart1_dev = NULL;
    for (int i = 0; i < 3; i++) {
        static const hwaddr uart_base[] = {
            PBL_UART0_BASE, PBL_UART1_BASE, PBL_UART2_BASE
        };
        static const int uart_irq[] = {
            PBL_IRQ_UART0, PBL_IRQ_UART1, PBL_IRQ_UART2
        };

        DeviceState *dev = qdev_new(TYPE_PEBBLE_SIMPLE_UART);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

        /* UART1 gets no chardev — pebble_control manages its CharBackend */
        if (i != 1) {
            qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        }
        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, uart_base[i]);
        sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(armv7m, uart_irq[i]));

        if (i == 1) {
            uart1_dev = dev;
        }
    }

    /* === Pebble Control Protocol on UART1 === */
    PebbleControl *pctrl = NULL;
    {
        Chardev *chr = serial_hd(1);
        if (chr && uart1_dev) {
            pctrl = pebble_control_create_generic(chr, uart1_dev);
        }
    }

    if (cfg->board_type == PBL_BOARD_CYBERDECK_EVT3 && pctrl) {
        s_cyberdeck_kbd.pctrl = pctrl;
        s_cyberdeck_kbd.button_state = 0;
        QemuInputHandlerState *ihs = qemu_input_handler_register(
            (DeviceState *)&s_cyberdeck_kbd, &cyberdeck_kbd_handler);
        qemu_input_handler_activate(ihs);
    }

    /* === Timers === */
    for (int i = 0; i < 2; i++) {
        static const hwaddr timer_base[] = {
            PBL_TIMER0_BASE, PBL_TIMER1_BASE
        };
        static const int timer_irq[] = {
            PBL_IRQ_TIMER0, PBL_IRQ_TIMER1
        };

        DeviceState *dev = qdev_new(TYPE_PEBBLE_GENERIC_TIMER);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

        qdev_connect_clock_in(dev, "clk", s->sysclk);
        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, timer_base[i]);
        sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(armv7m, timer_irq[i]));
    }

    /* === RTC === */
    {
        DeviceState *dev = qdev_new(TYPE_PEBBLE_GENERIC_RTC);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, PBL_RTC_BASE);
        sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(armv7m, PBL_IRQ_RTC));
    }

    /* === System Control === */
    {
        DeviceState *dev = qdev_new(TYPE_PEBBLE_SYSCTRL);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
        uint32_t features = 0;

        if (cfg->has_touch) features |= (1 << 0);
        if (cfg->has_audio) features |= (1 << 1);
        if (cfg->display_round) features |= (1 << 2);

        qdev_prop_set_uint32(dev, "board-id", cfg->board_id);
        qdev_prop_set_uint32(dev, "features", features);
        qdev_prop_set_uint32(dev, "display-width", cfg->display_width);
        qdev_prop_set_uint32(dev, "display-height", cfg->display_height);
        qdev_prop_set_uint32(dev, "display-format", cfg->display_bpp);

        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, PBL_SYSCTRL_BASE);
    }

    /* === Display === */
    {
        DeviceState *dev = qdev_new(TYPE_PEBBLE_DISPLAY);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

        qdev_prop_set_uint32(dev, "width", cfg->display_width);
        qdev_prop_set_uint32(dev, "height", cfg->display_height);
        qdev_prop_set_uint32(dev, "format", cfg->display_bpp);
        qdev_prop_set_bit(dev, "round-mask", cfg->display_round);

        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, PBL_DISPLAY_BASE);     /* registers */
        sysbus_mmio_map(sbd, 1, PBL_DISPLAY_FB_BASE);   /* framebuffer */
        sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(armv7m, PBL_IRQ_DISPLAY));
    }

    /* === External Flash (XIP) === */
    {
        DeviceState *dev = qdev_new(TYPE_PEBBLE_EXTFLASH);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

        qdev_prop_set_uint32(dev, "size", PBL_EXTFLASH_SIZE);

        /* Attach block device if user provided -drive if=mtd */
        DriveInfo *dinfo = drive_get(IF_MTD, 0, 0);
        if (dinfo) {
            qdev_prop_set_drive(dev, "drive", blk_by_legacy_dinfo(dinfo));
        }

        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, PBL_EXTFLASH_CTRL_BASE);  /* registers */
        sysbus_mmio_map(sbd, 1, PBL_EXTFLASH_BASE);        /* XIP memory */
    }

    /* === GPIO (Buttons) === */
    {
        DeviceState *dev = qdev_new(TYPE_PEBBLE_GPIO);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, PBL_GPIO_BASE);
        sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(armv7m, PBL_IRQ_GPIO));
    }

    /* === Touch Controller (Emery/Gabbro only) === */
    if (cfg->has_touch) {
        DeviceState *dev = qdev_new(TYPE_PEBBLE_TOUCH);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

        qdev_prop_set_uint32(dev, "display-width", cfg->display_width);
        qdev_prop_set_uint32(dev, "display-height", cfg->display_height);

        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, PBL_TOUCH_BASE);
        sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(armv7m, PBL_IRQ_TOUCH));
    } else {
        /* Non-touch boards: install a no-op absolute-pointer handler so the
         * UI does not grab the host cursor on click. */
        DeviceState *dev = qdev_new(TYPE_PEBBLE_NULL_INPUT);
        qdev_realize_and_unref(dev, NULL, &error_fatal);
    }

    /* === Audio DAC (Emery/Flint) === */
    if (cfg->has_audio) {
        DeviceState *dev = qdev_new(TYPE_PEBBLE_AUDIO);
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

        if (machine->audiodev) {
            qdev_prop_set_string(dev, "audiodev", machine->audiodev);
        }
        sysbus_realize_and_unref(sbd, &error_fatal);
        sysbus_mmio_map(sbd, 0, PBL_AUDIO_BASE);
        sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(armv7m, PBL_IRQ_AUDIO));
    }

    /* === QEMU Settings (RTC backup register 0) === */
    /* Firmware reads these flags to detect QEMU mode and configure behavior.
     * Written directly to the RTC MMIO region (backup reg 0 = offset 0x40). */
    {
#define QEMU_SETTING_FIRST_BOOT_LOGIC  0x00000001
#define QEMU_SETTING_START_CONNECTED   0x00000002
#define QEMU_SETTING_START_PLUGGED_IN  0x00000004
        uint32_t qemu_flags = QEMU_SETTING_START_CONNECTED;

        const char *env;
        env = getenv("PEBBLE_QEMU_FIRST_BOOT_LOGIC_ENABLE");
        if (env && atoi(env)) {
            qemu_flags |= QEMU_SETTING_FIRST_BOOT_LOGIC;
        }
        env = getenv("PEBBLE_QEMU_START_CONNECTED");
        if (env && !atoi(env)) {
            qemu_flags &= ~QEMU_SETTING_START_CONNECTED;
        }
        env = getenv("PEBBLE_QEMU_START_PLUGGED_IN");
        if (env && atoi(env)) {
            qemu_flags |= QEMU_SETTING_START_PLUGGED_IN;
        }

        /* Write to RTC backup register 0 via physical memory write */
        uint32_t flags_le = cpu_to_le32(qemu_flags);
        cpu_physical_memory_write(PBL_RTC_BASE + 0x40,
                                  &flags_le, sizeof(flags_le));
    }

    /* Load firmware */
    armv7m_load_kernel(s->armv7m.cpu, machine->kernel_filename,
                       0, cfg->flash_size);
}

/* ===== Machine class hierarchy ===== */

static void pbl_generic_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->init = pbl_generic_init;
    mc->max_cpus = 1;
    mc->ignore_memory_transaction_failures = true;
}

static void pbl_emery_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PblGenericMachineClass *pmc = PBL_GENERIC_MACHINE_CLASS(oc);
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m33"),
        NULL
    };

    mc->desc = board_cfg_emery.desc;
    mc->default_cpu_type = board_cfg_emery.cpu_type;
    mc->valid_cpu_types = valid_cpu_types;
    pmc->board_cfg = &board_cfg_emery;
    machine_add_audiodev_property(mc);
}

static void pbl_flint_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PblGenericMachineClass *pmc = PBL_GENERIC_MACHINE_CLASS(oc);
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m4"),
        NULL
    };

    mc->desc = board_cfg_flint.desc;
    mc->default_cpu_type = board_cfg_flint.cpu_type;
    mc->valid_cpu_types = valid_cpu_types;
    pmc->board_cfg = &board_cfg_flint;
    machine_add_audiodev_property(mc);
}

static void pbl_cyberdeck_evt3_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PblGenericMachineClass *pmc = PBL_GENERIC_MACHINE_CLASS(oc);
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m4"),
        NULL
    };

    mc->desc = board_cfg_cyberdeck_evt3.desc;
    mc->default_cpu_type = board_cfg_cyberdeck_evt3.cpu_type;
    mc->valid_cpu_types = valid_cpu_types;
    pmc->board_cfg = &board_cfg_cyberdeck_evt3;
    machine_add_audiodev_property(mc);
}

static void pbl_gabbro_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PblGenericMachineClass *pmc = PBL_GENERIC_MACHINE_CLASS(oc);
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m33"),
        NULL
    };

    mc->desc = board_cfg_gabbro.desc;
    mc->default_cpu_type = board_cfg_gabbro.cpu_type;
    mc->valid_cpu_types = valid_cpu_types;
    pmc->board_cfg = &board_cfg_gabbro;
}

/* ===== Type registration ===== */

static const TypeInfo pbl_generic_info = {
    .name          = TYPE_PBL_GENERIC_MACHINE,
    .parent        = TYPE_MACHINE,
    .abstract      = true,
    .instance_size = sizeof(PblGenericMachineState),
    .class_size    = sizeof(PblGenericMachineClass),
    .class_init    = pbl_generic_class_init,
};

static const TypeInfo pbl_emery_info = {
    .name          = MACHINE_TYPE_NAME("pebble-emery"),
    .parent        = TYPE_PBL_GENERIC_MACHINE,
    .class_init    = pbl_emery_class_init,
};

static const TypeInfo pbl_flint_info = {
    .name          = MACHINE_TYPE_NAME("pebble-flint"),
    .parent        = TYPE_PBL_GENERIC_MACHINE,
    .class_init    = pbl_flint_class_init,
};

static const TypeInfo pbl_cyberdeck_evt3_info = {
    .name          = MACHINE_TYPE_NAME("pebble-cyberdeck-evt3"),
    .parent        = TYPE_PBL_GENERIC_MACHINE,
    .class_init    = pbl_cyberdeck_evt3_class_init,
};

static const TypeInfo pbl_gabbro_info = {
    .name          = MACHINE_TYPE_NAME("pebble-gabbro"),
    .parent        = TYPE_PBL_GENERIC_MACHINE,
    .class_init    = pbl_gabbro_class_init,
};

static void pbl_generic_machine_init(void)
{
    type_register_static(&pbl_generic_info);
    type_register_static(&pbl_emery_info);
    type_register_static(&pbl_flint_info);
    type_register_static(&pbl_cyberdeck_evt3_info);
    type_register_static(&pbl_gabbro_info);
}

type_init(pbl_generic_machine_init)
