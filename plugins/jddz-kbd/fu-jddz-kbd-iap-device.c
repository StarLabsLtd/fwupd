/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "fu-jddz-kbd-common.h"
#include "fu-jddz-kbd-firmware.h"
#include "fu-jddz-kbd-iap-device.h"

struct _FuJddzKbdIapDevice {
	FuUsbDevice parent_instance;
};

G_DEFINE_TYPE(FuJddzKbdIapDevice, fu_jddz_kbd_iap_device, FU_TYPE_USB_DEVICE)

static GByteArray *
fu_jddz_kbd_iap_device_transfer(FuJddzKbdIapDevice *self, GByteArray *req, GError **error)
{
	gsize actual_len = 0;
	g_autoptr(GByteArray) res = g_byte_array_new();

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
		return NULL;
	}
	if (actual_len != req->len) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_WRITE,
			    "wrote 0x%zx bytes, expected 0x%x",
			    actual_len,
			    req->len);
		return NULL;
	}

	fu_byte_array_set_size(res, FU_JDDZ_KBD_IAP_PACKET_SIZE, 0x0);
	if (!fu_usb_device_interrupt_transfer(FU_USB_DEVICE(self),
					      FU_JDDZ_KBD_EP_IN,
					      res->data,
					      res->len,
					      &actual_len,
					      1000,
					      NULL,
					      error)) {
		g_prefix_error_literal(error, "failed to read response: ");
		return NULL;
	}
	if (actual_len != res->len) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_READ,
			    "read 0x%zx bytes, expected 0x%x",
			    actual_len,
			    res->len);
		return NULL;
	}
	fu_dump_raw(G_LOG_DOMAIN, "response", res->data, res->len);
	return g_steal_pointer(&res);
}

static gboolean
fu_jddz_kbd_iap_device_open(FuDevice *device, GError **error)
{
	FuJddzKbdIapDevice *self = FU_JDDZ_KBD_IAP_DEVICE(device);

	if (!FU_DEVICE_CLASS(fu_jddz_kbd_iap_device_parent_class)->open(device, error))
		return FALSE;
	return fu_usb_device_claim_interface(FU_USB_DEVICE(self),
					     0,
					     FU_USB_DEVICE_CLAIM_FLAG_KERNEL_DRIVER,
					     error);
}

static gboolean
fu_jddz_kbd_iap_device_setup(FuDevice *device, GError **error)
{
	FuJddzKbdIapDevice *self = FU_JDDZ_KBD_IAP_DEVICE(device);
	g_autoptr(GByteArray) req = NULL;
	g_autoptr(GByteArray) res = NULL;

	if (!FU_DEVICE_CLASS(fu_jddz_kbd_iap_device_parent_class)->setup(device, error))
		return FALSE;

	/* the IAP USB identity appears before its interrupt endpoints are ready */
	fu_device_sleep(device, 3000);
	req = fu_jddz_kbd_build_iap_init_request();
	res = fu_jddz_kbd_iap_device_transfer(self, req, error);
	if (res == NULL)
		return FALSE;
	return fu_jddz_kbd_validate_iap_init_response(res, error);
}

static gboolean
fu_jddz_kbd_iap_device_write_firmware(FuDevice *device,
				      FuFirmware *firmware,
				      FuProgress *progress,
				      FwupdInstallFlags flags,
				      GError **error)
{
	FuJddzKbdIapDevice *self = FU_JDDZ_KBD_IAP_DEVICE(device);
	g_autoptr(FuChunkArray) chunks = NULL;
	g_autoptr(FuInputStream) stream = NULL;
	g_autoptr(GByteArray) req = fu_jddz_kbd_build_iap_metadata_request();
	g_autoptr(GByteArray) res = NULL;

	res = fu_jddz_kbd_iap_device_transfer(self, req, error);
	if (res == NULL)
		return FALSE;
	if (!fu_jddz_kbd_validate_iap_metadata_response(res, error))
		return FALSE;

	stream = fu_firmware_get_stream(firmware, error);
	if (stream == NULL)
		return FALSE;
	chunks = fu_chunk_array_new_from_stream(stream,
						FU_CHUNK_ADDR_OFFSET_NONE,
						FU_CHUNK_PAGESZ_NONE,
						FU_JDDZ_KBD_BLOCK_SIZE,
						error);
	if (chunks == NULL)
		return FALSE;
	if (fu_chunk_array_length(chunks) != FU_JDDZ_KBD_BLOCK_COUNT) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_FILE,
			    "firmware has %u blocks, expected %u",
			    fu_chunk_array_length(chunks),
			    (guint)FU_JDDZ_KBD_BLOCK_COUNT);
		return FALSE;
	}

	fu_progress_set_id(progress, G_STRLOC);
	fu_progress_set_steps(progress, fu_chunk_array_length(chunks));
	for (guint i = 0; i < fu_chunk_array_length(chunks); i++) {
		g_autoptr(FuChunk) chk = fu_chunk_array_index(chunks, i, error);
		if (chk == NULL)
			return FALSE;
		g_clear_pointer(&req, g_byte_array_unref);
		g_clear_pointer(&res, g_byte_array_unref);
		req = fu_jddz_kbd_build_iap_block_request(i,
							  fu_chunk_get_data(chk),
							  fu_chunk_get_data_sz(chk),
							  error);
		if (req == NULL)
			return FALSE;
		res = fu_jddz_kbd_iap_device_transfer(self, req, error);
		if (res == NULL) {
			g_prefix_error(error,
				       "block 0x%x may have been written; refusing to retry: ",
				       i);
			return FALSE;
		}
		if (!fu_jddz_kbd_validate_iap_block_response(req, res, i, error))
			return FALSE;
		fu_progress_step_done(progress);
	}

	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG);
	return TRUE;
}

static void
fu_jddz_kbd_iap_device_set_progress(FuDevice *device, FuProgress *progress)
{
	fu_progress_set_id(progress, G_STRLOC);
	fu_progress_add_step(progress, FWUPD_STATUS_DECOMPRESSING, 0, "prepare-fw");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 0, "detach");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_WRITE, 85, "write");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_RESTART, 15, "attach");
	fu_progress_add_step(progress, FWUPD_STATUS_DEVICE_BUSY, 0, "reload");
}

static void
fu_jddz_kbd_iap_device_init(FuJddzKbdIapDevice *self)
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
fu_jddz_kbd_iap_device_class_init(FuJddzKbdIapDeviceClass *klass)
{
	FuDeviceClass *device_class = FU_DEVICE_CLASS(klass);
	device_class->open = fu_jddz_kbd_iap_device_open;
	device_class->setup = fu_jddz_kbd_iap_device_setup;
	device_class->write_firmware = fu_jddz_kbd_iap_device_write_firmware;
	device_class->set_progress = fu_jddz_kbd_iap_device_set_progress;
}
