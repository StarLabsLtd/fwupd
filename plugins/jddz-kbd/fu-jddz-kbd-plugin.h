/*
 * Copyright 2026 Star Labs Systems Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <fwupdplugin.h>

#define FU_TYPE_JDDZ_KBD_PLUGIN (fu_jddz_kbd_plugin_get_type())
G_DECLARE_FINAL_TYPE(FuJddzKbdPlugin, fu_jddz_kbd_plugin, FU, JDDZ_KBD_PLUGIN, FuPlugin)
