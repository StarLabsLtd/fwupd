/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "fu-jddz-kbd-common.h"

static GByteArray *
fu_jddz_kbd_packet_new(gsize size)
{
	GByteArray *buf = g_byte_array_new();
	fu_byte_array_set_size(buf, size, 0x0);
	return buf;
}

static gboolean
fu_jddz_kbd_validate_exact(GByteArray *buf,
			   const guint8 *expected,
			   gsize expected_sz,
			   const gchar *name,
			   GError **error)
{
	if (buf->len != expected_sz || memcmp(buf->data, expected, expected_sz) != 0) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "invalid %s response",
			    name);
		return FALSE;
	}
	return TRUE;
}

GByteArray *
fu_jddz_kbd_build_version_request(guint8 selector)
{
	g_autoptr(GByteArray) buf = fu_jddz_kbd_packet_new(FU_JDDZ_KBD_RUNTIME_PACKET_SIZE);
	buf->data[0] = 0xcf;
	buf->data[1] = 0x01;
	buf->data[2] = 0x90;
	buf->data[3] = 0x55;
	buf->data[4] = selector;
	return g_steal_pointer(&buf);
}

gchar *
fu_jddz_kbd_parse_version_response(GByteArray *buf, guint8 selector, GError **error)
{
	guint8 value_sz;

	if (buf->len != FU_JDDZ_KBD_RUNTIME_PACKET_SIZE || buf->data[0] != 0xcf ||
	    buf->data[2] != 0x90 || buf->data[3] != 0xaa) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "invalid selector 0x%02x response",
			    selector);
		return NULL;
	}
	value_sz = buf->data[1];
	if (value_sz == 0 || value_sz > 4) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "invalid selector 0x%02x length 0x%02x",
			    selector,
			    value_sz);
		return NULL;
	}
	if (value_sz == 1)
		return g_strdup_printf("%u", buf->data[4]);
	if (value_sz == 2)
		return g_strdup_printf("%u.%u", buf->data[4], buf->data[5]);
	if (value_sz == 3)
		return g_strdup_printf("%u.%u.%u", buf->data[4], buf->data[5], buf->data[6]);
	return g_strdup_printf("%u.%u.%u.%u",
			       buf->data[4],
			       buf->data[5],
			       buf->data[6],
			       buf->data[7]);
}

GByteArray *
fu_jddz_kbd_build_challenge_request(void)
{
	/* nocheck:magic */
	static const guint8 request[] = {
	    0xce, 0x08, 0x11, 0xb2, 0x4a, 0x44, 0x44, 0x5a, 0x54, 0x65, 0x63, 0x68,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	return g_byte_array_new_take(g_memdup2(request, sizeof(request)), sizeof(request));
}

gboolean
fu_jddz_kbd_validate_challenge_response(GByteArray *buf, GError **error)
{
	/* nocheck:magic */
	static const guint8 response[] = {
	    0xce, 0x08, 0x11, 0xb2, 0x9c, 0x18, 0x3f, 0x80, 0xf7, 0x86, 0xbd, 0x1d,
	    0xde, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	return fu_jddz_kbd_validate_exact(buf, response, sizeof(response), "challenge", error);
}

GByteArray *
fu_jddz_kbd_build_runtime_detach_request(void)
{
	g_autoptr(GByteArray) buf = fu_jddz_kbd_packet_new(FU_JDDZ_KBD_RUNTIME_PACKET_SIZE);
	buf->data[0] = 0xca;
	buf->data[2] = 0x10;
	buf->data[3] = 0x55;
	return g_steal_pointer(&buf);
}

GByteArray *
fu_jddz_kbd_build_iap_init_request(void)
{
	g_autoptr(GByteArray) buf = fu_jddz_kbd_packet_new(FU_JDDZ_KBD_IAP_PACKET_SIZE);
	buf->data[0] = 0xca;
	buf->data[2] = 0x10;
	buf->data[3] = 0x55;
	return g_steal_pointer(&buf);
}

gboolean
fu_jddz_kbd_validate_iap_init_response(GByteArray *buf, GError **error)
{
	static const guint8 response[FU_JDDZ_KBD_IAP_PACKET_SIZE] = {
	    0xca,
	    0x01,
	    0x10,
	    0xaa,
	    0x62,
	};
	return fu_jddz_kbd_validate_exact(buf,
					  response,
					  sizeof(response),
					  "IAP initialization",
					  error);
}

GByteArray *
fu_jddz_kbd_build_iap_metadata_request(void)
{
	/* nocheck:magic */
	static const guint8 request[FU_JDDZ_KBD_IAP_PACKET_SIZE] = {
	    0xca,
	    0x10,
	    0x20,
	    0x00,
	    0x55,
	    0xff,
	    0x02,
	    0x04,
	    0x00,
	    0x00,
	    0x40,
	    0x00,
	};
	return g_byte_array_new_take(g_memdup2(request, sizeof(request)), sizeof(request));
}

gboolean
fu_jddz_kbd_validate_iap_metadata_response(GByteArray *buf, GError **error)
{
	/* nocheck:magic */
	static const guint8 response[FU_JDDZ_KBD_IAP_PACKET_SIZE] = {
	    0xca,
	    0x00,
	    0x20,
	    0xaa,
	    0x01,
	    0x00,
	    0x40,
	    0x00,
	};
	return fu_jddz_kbd_validate_exact(buf, response, sizeof(response), "IAP metadata", error);
}

GByteArray *
fu_jddz_kbd_build_iap_block_request(guint idx, const guint8 *data, gsize data_sz, GError **error)
{
	guint16 address;
	gboolean is_final = idx == FU_JDDZ_KBD_BLOCK_COUNT - 1;
	g_autoptr(GByteArray) buf = NULL;

	if (idx >= FU_JDDZ_KBD_BLOCK_COUNT) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "block index 0x%x out of range",
			    idx);
		return NULL;
	}
	if (data_sz != FU_JDDZ_KBD_BLOCK_SIZE) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "block 0x%x has invalid size 0x%zx",
			    idx,
			    data_sz);
		return NULL;
	}

	buf = fu_jddz_kbd_packet_new(FU_JDDZ_KBD_IAP_PACKET_SIZE);
	if (is_final)
		memset(buf->data + 0x14, 0xff, FU_JDDZ_KBD_IAP_PACKET_SIZE - 0x14);
	address = FU_JDDZ_KBD_BLOCK_ADDRESS + idx;
	if (is_final)
		address |= 0x1000;
	buf->data[0] = 0xca;
	buf->data[1] = 0x10;
	buf->data[2] = (guint8)(address >> 8);
	buf->data[3] = (guint8)address;
	if (!fu_memcpy_safe(buf->data, buf->len, 0x4, data, data_sz, 0x0, data_sz, error))
		return NULL;
	return g_steal_pointer(&buf);
}

gboolean
fu_jddz_kbd_validate_iap_block_response(GByteArray *req, GByteArray *res, guint idx, GError **error)
{
	guint8 status = idx == FU_JDDZ_KBD_BLOCK_COUNT - 1 ? 0x30 : 0x20;
	g_autoptr(GByteArray) expected = fu_jddz_kbd_packet_new(FU_JDDZ_KBD_IAP_PACKET_SIZE);

	if (req->len != FU_JDDZ_KBD_IAP_PACKET_SIZE || res->len != FU_JDDZ_KBD_IAP_PACKET_SIZE) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "block 0x%x response has invalid size",
			    idx);
		return FALSE;
	}
	expected->data[0] = 0xca;
	expected->data[2] = status;
	expected->data[3] = 0xaa;
	if (!fu_memcpy_safe(expected->data,
			    expected->len,
			    0x4,
			    req->data,
			    req->len,
			    0x8,
			    0xc,
			    error))
		return FALSE;
	if (!fu_memcpy_safe(expected->data,
			    expected->len,
			    0x10,
			    req->data,
			    req->len,
			    0x10,
			    0x4,
			    error))
		return FALSE;
	if (!fu_memcpy_safe(expected->data,
			    expected->len,
			    0x14,
			    req->data,
			    req->len,
			    0x14,
			    0xc,
			    error))
		return FALSE;
	if (memcmp(res->data, expected->data, expected->len) != 0) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "block 0x%x acknowledgment mismatch",
			    idx);
		return FALSE;
	}
	return TRUE;
}
