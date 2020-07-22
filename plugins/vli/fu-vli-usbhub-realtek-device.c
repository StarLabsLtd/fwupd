/*
 * Copyright (C) 2017-2019 VIA Corporation
 * Copyright (C) 2019-2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-chunk.h"

#include "fu-vli-usbhub-device.h"
#include "fu-vli-usbhub-realtek-device.h"

struct _FuVliUsbhubRealtekDevice
{
	FuDevice		 parent_instance;
};

G_DEFINE_TYPE (FuVliUsbhubRealtekDevice, fu_vli_usbhub_realtek_device, FU_TYPE_DEVICE)

#define I2C_WRITE_REQUEST		0xB2
#define I2C_READ_REQUEST		0xA5

#define I2C_DELAY_AFTER_SEND		5000	/* us */

#define UC_FOREGROUND_SLAVE_ADDR	0x3A
#define UC_FOREGROUND_OPCODE		0x33
#define UC_FOREGROUND_ISP_DATA_OPCODE	0x34

#define ISP_DATA_BLOCKSIZE		30
#define ISP_PACKET_SIZE			32

static gboolean
fu_vli_usbhub_device_i2c_write (FuVliUsbhubDevice *self,
				guint8 slave_addr, guint8 sub_addr,
				guint8 *data, gsize datasz,
				GError **error)
{
	GUsbDevice *usb_device = fu_usb_device_get_dev (FU_USB_DEVICE (self));
	g_autofree guint8 *buf = g_malloc0 (datasz + 2);

	buf[0] = slave_addr;
	buf[1] = sub_addr;
	memcpy (buf + 2, data, datasz);

	if (g_getenv ("FWUPD_VLI_USBHUB_VERBOSE") != NULL)
		fu_common_dump_raw (G_LOG_DOMAIN, "I2cWriteData", buf, datasz + 2);
	if (!g_usb_device_control_transfer (usb_device,
					    G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
					    G_USB_DEVICE_REQUEST_TYPE_VENDOR,
					    G_USB_DEVICE_RECIPIENT_DEVICE,
					    I2C_WRITE_REQUEST, 0x0000, 0x0000,
					    buf, datasz + 2, NULL,
					    FU_VLI_DEVICE_TIMEOUT,
					    NULL, error)) {
		g_prefix_error (error,
				"failed to write I2C @0x%02x:%02x: ",
				slave_addr, sub_addr);
		return FALSE;
	}
	g_usleep (I2C_DELAY_AFTER_SEND);
	return TRUE;
}

static gboolean
fu_vli_usbhub_device_i2c_read (FuVliUsbhubDevice *self,
			       guint8 slave_addr, guint8 sub_addr,
			       guint8 *data, gsize datasz,
			       GError **error)
{
	GUsbDevice *usb_device = fu_usb_device_get_dev (FU_USB_DEVICE (self));
	if (!g_usb_device_control_transfer (usb_device,
					    G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
					    G_USB_DEVICE_REQUEST_TYPE_VENDOR,
					    G_USB_DEVICE_RECIPIENT_DEVICE,
					    I2C_READ_REQUEST, 0x0000,
					    (sub_addr << 8) + slave_addr,
					    data, datasz, NULL,
					    FU_VLI_DEVICE_TIMEOUT,
					    NULL, error)) {
		g_prefix_error (error, "failed to read I2C: ");
		return FALSE;
	}
	if (g_getenv ("FWUPD_VLI_USBHUB_VERBOSE") != NULL)
		fu_common_dump_raw (G_LOG_DOMAIN, "I2cReadData", data, datasz);
	return TRUE;

}

static gboolean
fu_vli_usbhub_device_realtek_read_status_raw (FuVliUsbhubRealtekDevice *self,
					      guint8 *status,
					      GError **error)
{
	FuVliUsbhubDevice *parent = FU_VLI_USBHUB_DEVICE (fu_device_get_parent (FU_DEVICE (self)));
	guint8 buf[1] = { 0x00 };
	if (!fu_vli_usbhub_device_i2c_read (parent,
					    UC_FOREGROUND_SLAVE_ADDR,
					    0x31,
					    buf, sizeof(buf),
					    error))
		return FALSE;
	if (status != NULL)
		*status = buf[0];
	return TRUE;
}

static gboolean
fu_vli_usbhub_device_realtek_read_status_cb (FuDevice *device,
					     gpointer user_data,
					     GError **error)
{
	FuVliUsbhubRealtekDevice *self = FU_VLI_USBHUB_REALTEK_DEVICE (device);
	guint8 status = 0xff;
	if (!fu_vli_usbhub_device_realtek_read_status_raw (self, &status, error))
		return FALSE;
	if (status == 0xBB) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "status was 0x%02x", status);
		return FALSE;
	}
	return TRUE;
}

static gboolean
fu_vli_usbhub_device_realtek_read_status (FuVliUsbhubRealtekDevice *self,
					  guint8 *status,
					  GError **error)
{
	return fu_device_retry (FU_DEVICE (self),
				fu_vli_usbhub_device_realtek_read_status_cb,
				4200, status, error);
}

static gboolean
fu_vli_usbhub_realtek_ensure_version_unlocked (FuVliUsbhubRealtekDevice *self,
					      GError **error)
{
	FuVliUsbhubDevice *parent = FU_VLI_USBHUB_DEVICE (fu_device_get_parent (FU_DEVICE (self)));
	guint8 buf_rep[7] = { 0x0 };
	guint8 buf_req[1] = { 0x04 };
	g_autofree gchar *version = NULL;

	if (!fu_vli_usbhub_device_i2c_write (parent,
					     UC_FOREGROUND_SLAVE_ADDR,
					     UC_FOREGROUND_OPCODE,
					     buf_req, sizeof(buf_req),
					     error)) {
		g_prefix_error (error, "failed to get version number: ");
		return FALSE;
	}

	/* FIXME: whoooa */
	g_usleep (300000);
	if (!fu_vli_usbhub_device_i2c_read (parent,
					    UC_FOREGROUND_SLAVE_ADDR,
					    0x00,
					    buf_rep, sizeof(buf_rep),
					    error)) {
		g_prefix_error (error, "failed to get version number: ");
		return FALSE;
	}

	/* set version */
	version = g_strdup_printf ("%u.%u", buf_rep[1], buf_rep[2]);
	fu_device_set_version (FU_DEVICE (self), version);
	return TRUE;
}

static gboolean
fu_vli_usbhub_realtek_device_setup (FuDevice *device, GError **error)
{
	FuVliUsbhubRealtekDevice *self = FU_VLI_USBHUB_REALTEK_DEVICE (device);
	g_autoptr(FuDeviceLocker) locker = NULL;

	/* get version */
	locker = fu_device_locker_new_full (device,
					    (FuDeviceLockerFunc) fu_device_detach,
					    (FuDeviceLockerFunc) fu_device_attach,
					    error);
	if (locker == NULL)
		return FALSE;
	if (!fu_vli_usbhub_realtek_ensure_version_unlocked (self, error))
		return FALSE;

	/* success */
	return TRUE;
}

static gboolean
fu_vli_usbhub_device_realtek_detach_raw (FuVliUsbhubRealtekDevice *self, GError **error)
{
	FuVliUsbhubDevice *parent = FU_VLI_USBHUB_DEVICE (fu_device_get_parent (FU_DEVICE (self)));
	guint8 buf[1] = { 0x03 };
	if (!fu_vli_usbhub_device_i2c_write (parent, 0x6A, 0x31, buf, sizeof(buf), error)) {
		g_prefix_error (error, "failed to detach: ");
		return FALSE;
	}
	return TRUE;
}

static gboolean
fu_vli_usbhub_device_realtek_detach_cb (FuDevice *device,
					gpointer user_data,
					GError **error)
{
	FuVliUsbhubRealtekDevice *self = FU_VLI_USBHUB_REALTEK_DEVICE (device);
	guint8 status = 0xff;
	if (!fu_vli_usbhub_device_realtek_detach_raw (self, error))
		return FALSE;
	if (!fu_vli_usbhub_device_realtek_read_status_raw (self, &status, error))
		return FALSE;
	if (status != 0x11) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "status was 0x%02x", status);
		return FALSE;
	}
	return TRUE;
}

static gboolean
fu_vli_usbhub_realtek_device_detach (FuDevice *device, GError **error)
{
	FuVliUsbhubDevice *parent = FU_VLI_USBHUB_DEVICE (fu_device_get_parent (device));
	g_autoptr(FuDeviceLocker) locker = NULL;

	/* open device */
	locker = fu_device_locker_new (parent, error);
	if (locker == NULL)
		return FALSE;
	return fu_device_retry (device,
				fu_vli_usbhub_device_realtek_detach_cb,
				100, NULL, error);
}

static gboolean
fu_vli_usbhub_realtek_device_attach (FuDevice *device, GError **error)
{
	FuVliUsbhubDevice *parent = FU_VLI_USBHUB_DEVICE (fu_device_get_parent (device));
	guint8 buf[1] = { 0x08 };
	g_autoptr(FuDeviceLocker) locker = NULL;

	/* open device */
	locker = fu_device_locker_new (parent, error);
	if (locker == NULL)
		return FALSE;
	if (!fu_vli_usbhub_device_i2c_write (parent,
					     UC_FOREGROUND_SLAVE_ADDR,
					     UC_FOREGROUND_OPCODE,
					     buf, sizeof(buf),
					     error)) {
		g_prefix_error (error, "failed to attach: ");
		return FALSE;
	}
	return FALSE;
}

static gboolean
fu_vli_usbhub_realtek_device_write_firmware (FuDevice *device,
					     FuFirmware *firmware,
					     FwupdInstallFlags flags,
					     GError **error)
{
	FuVliUsbhubDevice *parent = FU_VLI_USBHUB_DEVICE (fu_device_get_parent (device));
	FuVliUsbhubRealtekDevice *self = FU_VLI_USBHUB_REALTEK_DEVICE (device);
	const guint8 *fwbuf;
	gsize fwbufsz = 0;
	guint32 project_addr;
	guint8 project_id_count;
	guint8 read_buf[10] = { 0 };
	guint8 write_buf[ISP_PACKET_SIZE] = {0};
	g_autoptr(FuDeviceLocker) locker = NULL;
	g_autoptr(GBytes) fw = NULL;
	g_autoptr(GPtrArray) chunks = NULL;

	/* open device */
	locker = fu_device_locker_new (parent, error);
	if (locker == NULL)
		return FALSE;

	/* simple image */
	fw = fu_firmware_get_image_default_bytes (firmware, error);
	if (fw == NULL)
		return FALSE;
	fwbuf = g_bytes_get_data (fw, &fwbufsz);

	/* enable isp prior command */
	write_buf[0] = 0x01;
	write_buf[1] = 0x01;
	if (!fu_vli_usbhub_device_i2c_write (parent,
					     UC_FOREGROUND_SLAVE_ADDR,
					     UC_FOREGROUND_OPCODE,
					     write_buf,
					     2,
					     error)) {
		g_prefix_error (error, "failed to send enable isp cmd: ");
		return FALSE;
	}
	if (!fu_vli_usbhub_device_realtek_read_status (self, NULL, error))
		return FALSE;

	/* get project ID address */
	write_buf[0] = 0x02;
	if (!fu_vli_usbhub_device_i2c_write (parent,
					     UC_FOREGROUND_SLAVE_ADDR,
					     UC_FOREGROUND_OPCODE,
					     write_buf, 2,
					     error)) {
		g_prefix_error (error, "failed to send fw update start cmd: ");
		return FALSE;

	}

	/* read back 6 bytes data */
	g_usleep (I2C_DELAY_AFTER_SEND * 40);
	if (!fu_vli_usbhub_device_i2c_read (parent,
					    UC_FOREGROUND_SLAVE_ADDR,
					    0x31,
					    read_buf, 6,
					    error)) {
		g_prefix_error (error, "failed to read project ID: ");
		return FALSE;
	}
	if (read_buf[0] != 0x11) {
		g_prefix_error (error, "failed project ID with error 0x%02x", read_buf[0]);
		return FALSE;
	}

	/* verify project ID */
	project_addr = fu_common_read_uint32 (read_buf + 1, G_BIG_ENDIAN);
	project_id_count = read_buf[5];
	write_buf[0] = 0x03;
	if (!fu_memcpy_safe (write_buf, sizeof(write_buf), 0x1, /* dst */
			     fwbuf, fwbufsz, project_addr,	/* src */
			     project_id_count, error)) {
		g_prefix_error (error, "failed to read project ID from 0x%04x: ", project_addr);
		return FALSE;
	}
	if (!fu_vli_usbhub_device_i2c_write (parent,
					     UC_FOREGROUND_SLAVE_ADDR,
					     UC_FOREGROUND_OPCODE,
					     write_buf,
					     project_id_count + 1,
					     error)) {
		g_prefix_error (error, "failed to send fw update start cmd: ");
		return FALSE;
	}
	if (!fu_vli_usbhub_device_realtek_read_status (self, NULL, error))
		return FALSE;

	/* background FW update start command */
	write_buf[0] = 0x05;
	fu_common_write_uint16 (write_buf + 1, ISP_DATA_BLOCKSIZE, G_BIG_ENDIAN);
	if (!fu_vli_usbhub_device_i2c_write (parent,
					     UC_FOREGROUND_SLAVE_ADDR,
					     UC_FOREGROUND_OPCODE,
					     write_buf, 3,
					     error)) {
		g_prefix_error (error, "failed to send fw update start cmd: ");
		return FALSE;
	}

	/* send data */
	chunks = fu_chunk_array_new_from_bytes (fw,
						0x00,	/* start addr */
						0x00,	/* page_sz */
						ISP_DATA_BLOCKSIZE);
	for (guint i = 0; i < chunks->len; i++) {
		FuChunk *chk = g_ptr_array_index (chunks, i);
		if (!fu_vli_usbhub_device_realtek_read_status (self, NULL, error))
			return FALSE;
		if (!fu_vli_usbhub_device_i2c_write (parent,
						     UC_FOREGROUND_SLAVE_ADDR,
						     0x34,
						     (guint8 *) chk->data,
						     chk->data_sz,
						     error)) {
			g_prefix_error (error, "failed to write @0x%04x: ", chk->address);
			return FALSE;
		}

		/* update progress */
		fu_device_set_progress_full (device, (gsize) i, (gsize) chunks->len - 1);
	}

	/* update finish command */
	if (!fu_vli_usbhub_device_realtek_read_status (self, NULL, error))
		return FALSE;
	write_buf[0] = 0x06;
	if (!fu_vli_usbhub_device_i2c_write (parent,
					     UC_FOREGROUND_SLAVE_ADDR,
					     UC_FOREGROUND_OPCODE,
					     write_buf, 1,
					     error)) {
		g_prefix_error (error, "failed update finish cmd: ");
		return FALSE;
	}

	/* exit background-fw mode */
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_RESTART);
	fu_device_set_progress (device, 0);
	if (!fu_vli_usbhub_device_realtek_read_status (self, NULL, error))
		return FALSE;
	write_buf[0] = 0x07;
	if (!fu_vli_usbhub_device_i2c_write (parent,
					     UC_FOREGROUND_SLAVE_ADDR,
					     UC_FOREGROUND_OPCODE,
					     write_buf, 1,
					     error)) {
		g_prefix_error (error, "exit background-fw mode cmd: ");
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
fu_vli_usbhub_realtek_device_probe (FuDevice *device, GError **error)
{
	FuVliDeviceKind device_kind = FU_VLI_DEVICE_KIND_REALTEK;
	FuVliUsbhubDevice *parent = FU_VLI_USBHUB_DEVICE (fu_device_get_parent (device));
	g_autofree gchar *instance_id = NULL;

	fu_device_set_name (device, fu_vli_common_device_kind_to_string (device_kind));

	/* add instance ID */
	instance_id = g_strdup_printf ("USB\\VID_%04X&PID_%04X&I2C_%s",
				       fu_usb_device_get_vid (FU_USB_DEVICE (parent)),
				       fu_usb_device_get_pid (FU_USB_DEVICE (parent)),
				       fu_vli_common_device_kind_to_string (device_kind));
	fu_device_add_instance_id (device, instance_id);

	return TRUE;
}

static void
fu_vli_usbhub_realtek_device_init (FuVliUsbhubRealtekDevice *self)
{
	fu_device_add_icon (FU_DEVICE (self), "audio-card");
	fu_device_set_protocol (FU_DEVICE (self), "com.vli.i2c");
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_NO_GUID_MATCHING);
	fu_device_set_version_format (FU_DEVICE (self), FWUPD_VERSION_FORMAT_PAIR);
	fu_device_set_logical_id (FU_DEVICE (self), "I2C");
	fu_device_retry_set_delay (FU_DEVICE (self), 3); /* ms */
}

static void
fu_vli_usbhub_realtek_device_class_init (FuVliUsbhubRealtekDeviceClass *klass)
{
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	klass_device->probe = fu_vli_usbhub_realtek_device_probe;
	klass_device->setup = fu_vli_usbhub_realtek_device_setup;
	klass_device->attach = fu_vli_usbhub_realtek_device_attach;
	klass_device->detach = fu_vli_usbhub_realtek_device_detach;
	klass_device->write_firmware = fu_vli_usbhub_realtek_device_write_firmware;
}

FuDevice *
fu_vli_usbhub_realtek_device_new (FuVliUsbhubDevice *parent)
{
	FuVliUsbhubRealtekDevice *self = g_object_new (FU_TYPE_VLI_USBHUB_REALTEK_DEVICE,
						      "parent", parent,
						      NULL);
	return FU_DEVICE (self);
}
