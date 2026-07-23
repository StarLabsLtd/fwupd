/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "fu-jddz-kbd-common.h"
#include "fu-jddz-kbd-firmware.h"

static GByteArray *
fu_jddz_kbd_test_array_from_hex(const gchar *hex)
{
	g_autoptr(GError) error = NULL;
	GByteArray *buf = fu_byte_array_from_string(hex, &error);
	g_assert_no_error(error);
	g_assert_nonnull(buf);
	return buf;
}

static void
fu_jddz_kbd_protocol_runtime_func(void)
{
	g_autoptr(GByteArray) req = fu_jddz_kbd_build_version_request(0x1);
	g_autoptr(GByteArray) res =
	    fu_jddz_kbd_test_array_from_hex("cf0490aa0201016200000000000000000000000000000000");
	g_autoptr(GByteArray) challenge_req = fu_jddz_kbd_build_challenge_request();
	g_autoptr(GByteArray) challenge_res =
	    fu_jddz_kbd_test_array_from_hex("ce0811b29c183f80f786bd1dde0000000000000000000000");
	g_autoptr(GByteArray) detach_req = fu_jddz_kbd_build_runtime_detach_request();
	g_autoptr(GByteArray) invalid_res = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *version = NULL;

	g_assert_cmpmem(req->data,
			req->len,
			"\xcf\x01\x90\x55\x01\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
			FU_JDDZ_KBD_RUNTIME_PACKET_SIZE);
	version = fu_jddz_kbd_parse_version_response(res, 0x1, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(version, ==, "2.1.1.98");
	g_assert_cmpmem(challenge_req->data,
			challenge_req->len,
			"\xce\x08\x11\xb2JDDZTech\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00",
			FU_JDDZ_KBD_RUNTIME_PACKET_SIZE);
	g_assert_true(fu_jddz_kbd_validate_challenge_response(challenge_res, &error));
	g_assert_no_error(error);
	g_assert_cmpmem(detach_req->data,
			detach_req->len,
			"\xca\x00\x10\x55\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
			FU_JDDZ_KBD_RUNTIME_PACKET_SIZE);

	invalid_res = g_byte_array_new_take(g_memdup2(challenge_res->data, challenge_res->len),
					    challenge_res->len);
	invalid_res->data[4] ^= 0x1;
	g_assert_false(fu_jddz_kbd_validate_challenge_response(invalid_res, &error));
	g_assert_error(error, FWUPD_ERROR, FWUPD_ERROR_INVALID_DATA);
	g_clear_error(&error);

	invalid_res->data[0] = 0xcf;
	invalid_res->data[1] = 0x5;
	invalid_res->data[2] = 0x90;
	invalid_res->data[3] = 0xaa;
	g_clear_pointer(&version, g_free);
	version = fu_jddz_kbd_parse_version_response(invalid_res, 0x1, &error);
	g_assert_null(version);
	g_assert_error(error, FWUPD_ERROR, FWUPD_ERROR_INVALID_DATA);
}

static void
fu_jddz_kbd_protocol_iap_func(void)
{
	static const guint8 block_data[FU_JDDZ_KBD_BLOCK_SIZE] = {
	    0x54,
	    0xdf,
	    0x00,
	    0xf0,
	    0xff,
	    0xff,
	    0xff,
	    0xff,
	    0x5a,
	    0x42,
	    0x99,
	    0xdf,
	    0x18,
	    0xf0,
	    0xff,
	    0xff,
	};
	static const guint8 final_data[FU_JDDZ_KBD_BLOCK_SIZE] = {
	    0x0a,
	    0x03,
	    0x4a,
	    0x00,
	    0x44,
	    0x00,
	    0x44,
	    0x00,
	    0x5a,
	    0x00,
	    0x04,
	    0x03,
	    0x09,
	    0x04,
	    0x00,
	    0xff,
	};
	g_autoptr(GByteArray) req = NULL;
	g_autoptr(GByteArray) init_req = fu_jddz_kbd_build_iap_init_request();
	g_autoptr(GByteArray) init_res =
	    fu_jddz_kbd_test_array_from_hex("ca0110aa620000000000000000000000"
					    "00000000000000000000000000000000");
	g_autoptr(GByteArray) metadata_req = fu_jddz_kbd_build_iap_metadata_request();
	g_autoptr(GByteArray) metadata_res =
	    fu_jddz_kbd_test_array_from_hex("ca0020aa010040000000000000000000"
					    "00000000000000000000000000000000");
	g_autoptr(GByteArray) res =
	    fu_jddz_kbd_test_array_from_hex("ca0020aaffffffff5a4299df18f0ffff"
					    "18f0ffff000000000000000000000000");
	g_autoptr(GByteArray) final_req = NULL;
	g_autoptr(GByteArray) final_res =
	    fu_jddz_kbd_test_array_from_hex("ca0030aa440044005a000403090400ff"
					    "090400ffffffffffffffffffffffffff");
	g_autoptr(GByteArray) invalid_res = NULL;
	g_autoptr(GError) error = NULL;

	g_assert_cmpmem(init_req->data,
			init_req->len,
			"\xca\x00\x10\x55\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
			FU_JDDZ_KBD_IAP_PACKET_SIZE);
	g_assert_true(fu_jddz_kbd_validate_iap_init_response(init_res, &error));
	g_assert_no_error(error);
	g_assert_cmpmem(metadata_req->data,
			metadata_req->len,
			"\xca\x10\x20\x00\x55\xff\x02\x04\x00\x00\x40\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
			FU_JDDZ_KBD_IAP_PACKET_SIZE);
	g_assert_true(fu_jddz_kbd_validate_iap_metadata_response(metadata_res, &error));
	g_assert_no_error(error);

	req = fu_jddz_kbd_build_iap_block_request(0, block_data, sizeof(block_data), &error);
	g_assert_no_error(error);
	g_assert_nonnull(req);
	g_assert_true(fu_jddz_kbd_validate_iap_block_response(req, res, 0, &error));
	g_assert_no_error(error);
	invalid_res = g_byte_array_new_take(g_memdup2(res->data, res->len), res->len);
	invalid_res->data[4] ^= 0x1;
	g_assert_false(fu_jddz_kbd_validate_iap_block_response(req, invalid_res, 0, &error));
	g_assert_error(error, FWUPD_ERROR, FWUPD_ERROR_INVALID_DATA);
	g_clear_error(&error);

	final_req = fu_jddz_kbd_build_iap_block_request(FU_JDDZ_KBD_BLOCK_COUNT - 1,
							final_data,
							sizeof(final_data),
							&error);
	g_assert_no_error(error);
	g_assert_nonnull(final_req);
	g_assert_cmphex(final_req->data[2], ==, 0x33);
	g_assert_cmphex(final_req->data[3], ==, 0xff);
	for (guint i = 0x14; i < final_req->len; i++)
		g_assert_cmphex(final_req->data[i], ==, 0xff);
	g_assert_true(fu_jddz_kbd_validate_iap_block_response(final_req,
							      final_res,
							      FU_JDDZ_KBD_BLOCK_COUNT - 1,
							      &error));
	g_assert_no_error(error);

	g_clear_pointer(&req, g_byte_array_unref);
	req = fu_jddz_kbd_build_iap_block_request(FU_JDDZ_KBD_BLOCK_COUNT,
						  block_data,
						  sizeof(block_data),
						  &error);
	g_assert_null(req);
	g_assert_error(error, FWUPD_ERROR, FWUPD_ERROR_INVALID_DATA);
	g_clear_error(&error);
	req = fu_jddz_kbd_build_iap_block_request(0, block_data, sizeof(block_data) - 1, &error);
	g_assert_null(req);
	g_assert_error(error, FWUPD_ERROR, FWUPD_ERROR_INVALID_DATA);
}

static void
fu_jddz_kbd_firmware_parse_func(void)
{
	static const guint8 descriptor[] = {
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
	    0x01,
	    0x02,
	    0x00,
	    0x02,
	    0x00,
	    0x01,
	};
	g_autoptr(GByteArray) buf = g_byte_array_sized_new(FU_JDDZ_KBD_FIRMWARE_SIZE);
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(GBytes) blob_written = NULL;
	g_autoptr(FuFirmware) firmware = fu_jddz_kbd_firmware_new();
	g_autoptr(GError) error = NULL;
	gboolean ret;

	fu_byte_array_set_size(buf, FU_JDDZ_KBD_FIRMWARE_SIZE, 0xff);
	g_assert_true(fu_memcpy_safe(buf->data,
				     buf->len,
				     0x3fca,
				     descriptor,
				     sizeof(descriptor),
				     0x0,
				     sizeof(descriptor),
				     &error));
	g_assert_no_error(error);
	blob = g_byte_array_free_to_bytes(g_steal_pointer(&buf));
	ret =
	    fu_firmware_parse_bytes(firmware, blob, 0x0, FU_FIRMWARE_PARSE_FLAG_NO_SEARCH, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	g_assert_cmphex(fu_firmware_get_version_raw(firmware), ==, 0x0201);
	g_assert_cmpstr(fu_firmware_get_version(firmware), ==, "2.1");
	blob_written = fu_firmware_write(firmware, &error);
	g_assert_no_error(error);
	g_assert_nonnull(blob_written);
	g_assert_true(g_bytes_equal(blob, blob_written));
}

static void
fu_jddz_kbd_firmware_invalid_func(void)
{
	static const guint8 descriptor[] = {
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
	    0x01,
	    0x02,
	    0x00,
	    0x02,
	    0x00,
	    0x01,
	};
	g_autoptr(GByteArray) buf = g_byte_array_sized_new(FU_JDDZ_KBD_FIRMWARE_SIZE);
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(FuFirmware) firmware = fu_jddz_kbd_firmware_new();
	g_autoptr(GError) error = NULL;

	fu_byte_array_set_size(buf, FU_JDDZ_KBD_FIRMWARE_SIZE, 0xff);
	g_assert_true(fu_memcpy_safe(buf->data,
				     buf->len,
				     0x100,
				     descriptor,
				     sizeof(descriptor),
				     0x0,
				     sizeof(descriptor),
				     &error));
	g_assert_no_error(error);
	g_assert_true(fu_memcpy_safe(buf->data,
				     buf->len,
				     0x200,
				     descriptor,
				     sizeof(descriptor),
				     0x0,
				     sizeof(descriptor),
				     &error));
	g_assert_no_error(error);
	blob = g_byte_array_free_to_bytes(g_steal_pointer(&buf));
	g_assert_false(
	    fu_firmware_parse_bytes(firmware, blob, 0x0, FU_FIRMWARE_PARSE_FLAG_NO_SEARCH, &error));
	g_assert_error(error, FWUPD_ERROR, FWUPD_ERROR_INVALID_FILE);
	g_clear_error(&error);

	g_clear_object(&firmware);
	firmware = fu_jddz_kbd_firmware_new();
	blob = g_bytes_new_static(descriptor, sizeof(descriptor));
	g_assert_false(
	    fu_firmware_parse_bytes(firmware, blob, 0x0, FU_FIRMWARE_PARSE_FLAG_NO_SEARCH, &error));
	g_assert_error(error, FWUPD_ERROR, FWUPD_ERROR_INVALID_FILE);
}

int
main(int argc, char **argv)
{
	(void)g_setenv("G_TEST_SRCDIR", SRCDIR, FALSE);
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/jddz-kbd/protocol/runtime", fu_jddz_kbd_protocol_runtime_func);
	g_test_add_func("/jddz-kbd/protocol/iap", fu_jddz_kbd_protocol_iap_func);
	g_test_add_func("/jddz-kbd/firmware/parse", fu_jddz_kbd_firmware_parse_func);
	g_test_add_func("/jddz-kbd/firmware/invalid", fu_jddz_kbd_firmware_invalid_func);
	return g_test_run();
}
