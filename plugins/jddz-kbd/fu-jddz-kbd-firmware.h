/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <fwupdplugin.h>

#define FU_TYPE_JDDZ_KBD_FIRMWARE (fu_jddz_kbd_firmware_get_type())
G_DECLARE_FINAL_TYPE(FuJddzKbdFirmware, fu_jddz_kbd_firmware, FU, JDDZ_KBD_FIRMWARE, FuFirmware)

FuFirmware *
fu_jddz_kbd_firmware_new(void);
