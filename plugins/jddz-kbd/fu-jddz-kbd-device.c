/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "fu-jddz-kbd-common.h"
#include "fu-jddz-kbd-device.h"
#include "fu-jddz-kbd-firmware.h"

struct _FuJddzKbdDevice {
	FuUsbDevice parent_instance;
	gchar *component_version;
};

G_DEFINE_TYPE(FuJddzKbdDevice, fu_jddz_kbd_device, FU_TYPE_USB_DEVICE)

static gboolean
fu_jddz_kbd_device_transfer(FuJddzKbdDevice *self,
			    GByteArray *req,
			    GByteArray **res,
			    GError **error)
{
	gsize actual_len = 0;

	fu_dump_raw(G_LOG_DOMAIN, "request", req->data, req->len);
	if (!fu_usb_device_interrupt_transfer(FU_USB_DEVICE(self),
					      FU_JDDZ_KBD_EP_OUT,
					      req->data,
					      req->len,
					      &actual_len,
					      1000,
					      NULL,
					      error)) {
		g_prefix_error_literal(error, "failed to write request: ");
		return FALSE;
	}
	if (actual_len != req->len) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_WRITE,
			    "wrote 0x%zx bytes, expected 0x%x",
			    actual_len,
			    req->len);
		return FALSE;
	}
	if (res == NULL)
		return TRUE;

	*res = g_byte_array_new();
	fu_byte_array_set_size(*res, FU_JDDZ_KBD_RUNTIME_PACKET_SIZE, 0x0);
	if (!fu_usb_device_interrupt_transfer(FU_USB_DEVICE(self),
					      FU_JDDZ_KBD_EP_IN,
					      (*res)->data,
					      (*res)->len,
					      &actual_len,
					      1000,
					      NULL,
					      error)) {
		g_prefix_error_literal(error, "failed to read response: ");
		return FALSE;
	}
	if (actual_len != (*res)->len) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_READ,
			    "read 0x%zx bytes, expected 0x%x",
			    actual_len,
			    (*res)->len);
		return FALSE;
	}
	fu_dump_raw(G_LOG_DOMAIN, "response", (*res)->data, (*res)->len);
	return TRUE;
}

static void
fu_jddz_kbd_device_to_string(FuDevice *device, guint idt, GString *str)
{
	FuJddzKbdDevice *self = FU_JDDZ_KBD_DEVICE(device);
	fwupd_codec_string_append(str, idt, "ComponentVersion", self->component_version);
}

static gboolean
fu_jddz_kbd_device_open(FuDevice *device, GError **error)
{
	FuJddzKbdDevice *self = FU_JDDZ_KBD_DEVICE(device);

	if (!FU_DEVICE_CLASS(fu_jddz_kbd_device_parent_class)->open(device, error))
		return FALSE;
	return fu_usb_device_claim_interface(FU_USB_DEVICE(self),
					     2,
					     FU_USB_DEVICE_CLAIM_FLAG_KERNEL_DRIVER,
					     error);
}

static gboolean
fu_jddz_kbd_device_setup(FuDevice *device, GError **error)
{
	FuJddzKbdDevice *self = FU_JDDZ_KBD_DEVICE(device);
	g_autoptr(GByteArray) req = NULL;
	g_autoptr(GByteArray) res = NULL;
	g_autofree gchar *version = NULL;

	if (!FU_DEVICE_CLASS(fu_jddz_kbd_device_parent_class)->setup(device, error))
		return FALSE;
	req = fu_jddz_kbd_build_version_request(0x1);
	if (!fu_jddz_kbd_device_transfer(self, req, &res, error))
		return FALSE;
	version = fu_jddz_kbd_parse_version_response(res, 0x1, error);
	if (version == NULL)
		return FALSE;
	g_free(self->component_version);
	self->component_version = g_steal_pointer(&version);
	return TRUE;
}

static gboolean
fu_jddz_kbd_device_detach(FuDevice *device, FuProgress *progress, GError **error)
{
	FuJddzKbdDevice *self = FU_JDDZ_KBD_DEVICE(device);
	g_autoptr(GByteArray) req = fu_jddz_kbd_build_challenge_request();
	g_autoptr(GByteArray) res = NULL;
	g_autoptr(GError) error_local = NULL;

	if (!fu_jddz_kbd_device_transfer(self, req, &res, error))
		return FALSE;
	if (!fu_jddz_kbd_validate_challenge_response(res, error))
		return FALSE;
	fu_device_sleep(device, 1000);

	g_clear_pointer(&req, g_byte_array_unref);
	req = fu_jddz_kbd_build_runtime_detach_request();
	if (!fu_jddz_kbd_device_transfer(self, req, NULL, &error_local)) {
		if (g_error_matches(error_local, FWUPD_ERROR, FWUPD_ERROR_NOT_FOUND) ||
		    g_error_matches(error_local, FWUPD_ERROR, FWUPD_ERROR_WRITE)) {
			g_debug("ignoring detach error: %s", error_local->message);
		} else {
			g_propagate_error(error, g_steal_pointer(&error_local));
			return FALSE;
		}
	}
	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG);
	return TRUE;
}

static void
fu_jddz_kbd_device_set_progress(FuDevice *device, FuProgress *progress)
{
	fu_progress_set_id(progress, G_STRLOC);
	fu_progress_add_step(progress, FWUPD_STATUS_DECOMPRESSING, 0, "prepare-fw");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 20, "detach");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_WRITE, 65, "write");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 15, "attach");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_BUSY, 0, "reload");
}

static void
fu_jddz_kbd_device_finalize(GObject *obj)
{
	FuJddzKbdDevice *self = FU_JDDZ_KBD_DEVICE(obj);
	g_free(self->component_version);
	G_OBJECT_CLASS(fu_jddz_kbd_device_parent_class)->finalize(obj);
}

static void
fu_jddz_kbd_device_init(FuJddzKbdDevice *self)
{
	fu_device_set_name(FU_DEVICE(self), "StarLite Magnetic Keyboard");
	fu_device_set_remove_delay(FU_DEVICE(self), FU_DEVICE_REMOVE_DELAY_RE_ENUMERATE);
	fu_device_set_version_format(FU_DEVICE(self), FWUPD_VERSION_FORMAT_BCD);
	fu_device_add_protocol(FU_DEVICE(self), "com.jddz.kbd");
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_add_flag(FU_DEVICE(self), FWUPD_DEVICE_FLAG_UNSIGNED_PAYLOAD);
	fu_device_add_icon(FU_DEVICE(self), FU_DEVICE_ICON_INPUT_KEYBOARD);
	fu_device_set_firmware_gtype(FU_DEVICE(self), FU_TYPE_JDDZ_KBD_FIRMWARE);
}

static void
fu_jddz_kbd_device_class_init(FuJddzKbdDeviceClass *klass)
{
	FuDeviceClass *device_class = FU_DEVICE_CLASS(klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	device_class->open = fu_jddz_kbd_device_open;
	device_class->setup = fu_jddz_kbd_device_setup;
	device_class->detach = fu_jddz_kbd_device_detach;
	device_class->to_string = fu_jddz_kbd_device_to_string;
	device_class->set_progress = fu_jddz_kbd_device_set_progress;
	object_class->finalize = fu_jddz_kbd_device_finalize;
}
