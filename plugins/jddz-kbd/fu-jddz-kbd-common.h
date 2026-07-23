/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <fwupdplugin.h>

#define FU_JDDZ_KBD_RUNTIME_PACKET_SIZE 0x18
#define FU_JDDZ_KBD_IAP_PACKET_SIZE	0x20
#define FU_JDDZ_KBD_FIRMWARE_SIZE	0x4000
#define FU_JDDZ_KBD_BLOCK_SIZE		0x10
#define FU_JDDZ_KBD_BLOCK_COUNT		(FU_JDDZ_KBD_FIRMWARE_SIZE / FU_JDDZ_KBD_BLOCK_SIZE)
#define FU_JDDZ_KBD_BLOCK_ADDRESS	0x2000

#define FU_JDDZ_KBD_EP_OUT 0x03
#define FU_JDDZ_KBD_EP_IN  0x83

GByteArray *
fu_jddz_kbd_build_version_request(guint8 selector);
gchar *
fu_jddz_kbd_parse_version_response(GByteArray *buf, guint8 selector, GError **error);
GByteArray *
fu_jddz_kbd_build_challenge_request(void);
gboolean
fu_jddz_kbd_validate_challenge_response(GByteArray *buf, GError **error);
GByteArray *
fu_jddz_kbd_build_runtime_detach_request(void);
GByteArray *
fu_jddz_kbd_build_iap_init_request(void);
gboolean
fu_jddz_kbd_validate_iap_init_response(GByteArray *buf, GError **error);
GByteArray *
fu_jddz_kbd_build_iap_metadata_request(void);
gboolean
fu_jddz_kbd_validate_iap_metadata_response(GByteArray *buf, GError **error);
GByteArray *
fu_jddz_kbd_build_iap_block_request(guint idx, const guint8 *data, gsize data_sz, GError **error);
gboolean
fu_jddz_kbd_validate_iap_block_response(GByteArray *req,
					GByteArray *res,
					guint idx,
					GError **error);
