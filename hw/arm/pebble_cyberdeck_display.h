/*
 * Cyberdeck Sharp MIP display mux — public interface.
 */
#pragma once

#include "sysemu/char.h"
#include "hw/qdev-core.h"

/*
 * Set the K230 serial chardev on the display device before calling
 * qdev_init_nofail(). The chardev carries Sharp protocol bytes from the K230
 * Linux sharp-drm driver to the K230 framebuffer in the mux device.
 *
 * @dev: the DeviceState returned by ssi_create_slave_no_init() with
 *       type "sharp-mip-cyberdeck-mux"
 * @chr: serial_hds[4] from the wscript K230 display socket
 */
void cyberdeck_mux_set_k230_chr(DeviceState *dev, CharDriverState *chr);
