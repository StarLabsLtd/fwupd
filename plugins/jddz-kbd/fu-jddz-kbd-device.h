/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <fwupdplugin.h>

#define FU_TYPE_JDDZ_KBD_DEVICE (fu_jddz_kbd_device_get_type())
G_DECLARE_FINAL_TYPE(FuJddzKbdDevice, fu_jddz_kbd_device, FU, JDDZ_KBD_DEVICE, FuUsbDevice)
