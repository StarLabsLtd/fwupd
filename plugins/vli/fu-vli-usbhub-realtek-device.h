/*
 * Copyright (C) 2019-2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "fu-plugin.h"

#define FU_TYPE_VLI_USBHUB_REALTEK_DEVICE (fu_vli_usbhub_realtek_device_get_type ())
G_DECLARE_FINAL_TYPE (FuVliUsbhubRealtekDevice, fu_vli_usbhub_realtek_device, FU, VLI_USBHUB_REALTEK_DEVICE, FuDevice)

struct _FuVliUsbhubRealtekDeviceClass
{
	FuDeviceClass		parent_class;
};

FuDevice	*fu_vli_usbhub_realtek_device_new	(FuVliUsbhubDevice	*parent);
