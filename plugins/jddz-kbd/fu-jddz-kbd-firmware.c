/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "fu-jddz-kbd-common.h"
#include "fu-jddz-kbd-firmware.h"

struct _FuJddzKbdFirmware {
	FuFirmware parent_instance;
};

G_DEFINE_TYPE(FuJddzKbdFirmware, fu_jddz_kbd_firmware, FU_TYPE_FIRMWARE)

static gboolean
fu_jddz_kbd_firmware_parse(FuFirmware *firmware,
			   FuInputStream *stream,
			   FuFirmwareParseFlags flags,
			   GError **error)
{
	/* nocheck:magic */
	static const guint8 descriptor_prefix[] = {
	    0x12,
	    0x01,
	    0x10,
	    0x01,
	    0x00,
	    0x00,
	    0x00,
	    0x20,
	    0x4f,
	    0x1c,
	    0x7f,
	    0x00,
	};
	/* nocheck:magic */
	static const guint8 descriptor_suffix[] = {
	    0x00,
	    0x02,
	    0x00,
	    0x01,
	};
	gsize bufsz = 0;
	gsize descriptor_offset = G_MAXSIZE;
	guint matches = 0;
	guint16 version_raw;
	g_autoptr(GBytes) blob = NULL;
	const guint8 *buf;

	if (!fu_input_stream_size(stream, &bufsz, error))
		return FALSE;
	if (bufsz != FU_JDDZ_KBD_FIRMWARE_SIZE) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_FILE,
			    "firmware size 0x%zx, expected 0x%x",
			    bufsz,
			    (guint)FU_JDDZ_KBD_FIRMWARE_SIZE);
		return FALSE;
	}
	blob = fu_input_stream_read_bytes(stream, 0x0, bufsz, NULL, error);
	if (blob == NULL)
		return FALSE;
	buf = g_bytes_get_data(blob, NULL);
	for (gsize i = 0; i <= bufsz - 0x12; i++) {
		if (memcmp(buf + i, descriptor_prefix, sizeof(descriptor_prefix)) != 0)
			continue;
		if (memcmp(buf + i + 0xe, descriptor_suffix, sizeof(descriptor_suffix)) != 0)
			continue;
		descriptor_offset = i;
		matches++;
	}
	if (matches != 1) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_FILE,
			    "found %u matching USB device descriptors, expected 1",
			    matches);
		return FALSE;
	}

	version_raw =
	    (guint16)buf[descriptor_offset + 0xc] | ((guint16)buf[descriptor_offset + 0xd] << 8);
	fu_firmware_set_version_raw(firmware, version_raw);
	return fu_firmware_set_stream(firmware, stream, error);
}

static GByteArray *
fu_jddz_kbd_firmware_write(FuFirmware *firmware, GError **error)
{
	g_autoptr(GBytes) blob = fu_firmware_get_bytes_with_patches(firmware, error);

	if (blob == NULL)
		return NULL;
	return g_bytes_unref_to_array(g_steal_pointer(&blob));
}

static void
fu_jddz_kbd_firmware_init(FuJddzKbdFirmware *self)
{
	fu_firmware_set_version_format(FU_FIRMWARE(self), FWUPD_VERSION_FORMAT_BCD);
}

static gchar *
fu_jddz_kbd_firmware_convert_version(FuFirmware *firmware, guint64 version_raw)
{
	return fu_version_from_uint16((guint16)version_raw,
				      fu_firmware_get_version_format(firmware));
}

static void
fu_jddz_kbd_firmware_class_init(FuJddzKbdFirmwareClass *klass)
{
	FuFirmwareClass *firmware_class = FU_FIRMWARE_CLASS(klass);
	firmware_class->parse = fu_jddz_kbd_firmware_parse;
	firmware_class->write = fu_jddz_kbd_firmware_write;
	firmware_class->convert_version = fu_jddz_kbd_firmware_convert_version;
	fu_firmware_set_size_max(firmware_class, FU_JDDZ_KBD_FIRMWARE_SIZE);
}

FuFirmware *
fu_jddz_kbd_firmware_new(void)
{
	return FU_FIRMWARE(g_object_new(FU_TYPE_JDDZ_KBD_FIRMWARE, NULL));
}
