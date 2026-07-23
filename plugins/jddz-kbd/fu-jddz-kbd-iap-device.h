/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <fwupdplugin.h>

#define FU_TYPE_JDDZ_KBD_IAP_DEVICE (fu_jddz_kbd_iap_device_get_type())
G_DECLARE_FINAL_TYPE(FuJddzKbdIapDevice,
		     fu_jddz_kbd_iap_device,
		     FU,
		     JDDZ_KBD_IAP_DEVICE,
		     FuUsbDevice)
