/*
 * Copyright 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <fwupdplugin.h>

#include "fu-bios-settings-private.h"
#include "fu-context-private.h"

static void
fu_bios_settings_load_func(void)
{
	gboolean ret;
	gint integer;
	const gchar *tmp;
	GPtrArray *values;
	FwupdBiosSetting *setting;
	FwupdBiosSettingKind kind;
	g_autofree gchar *base_dir = NULL;
	g_autofree gchar *quirks_dir = NULL;
	g_autofree gchar *test_dir = NULL;
	g_autoptr(FuContext) ctx = fu_context_new();
	g_autoptr(FuProgress) progress = fu_progress_new(G_STRLOC);
	g_autoptr(GError) error = NULL;
	g_autoptr(FuBiosSettings) p620_6_3_settings = NULL;
	g_autoptr(GPtrArray) p620_6_3_items = NULL;

#ifdef _WIN32
	/* BIOS settings are Linux-specific (require sysfs). The test data directory
	 * "AlarmDate(MM\DD\YYYY)" was also causing repository clone failures on Windows,
	 * which has been fixed by renaming it to "AlarmDate-MM-DD-YYYY" */
	g_test_skip("BIOS settings not supported on Windows");
	return;
#endif

	/* ensure the data directory is actually present for the test */
	base_dir = g_test_build_filename(G_TEST_DIST, "tests", "bios-attrs", NULL);
	if (!g_file_test(base_dir, G_FILE_TEST_EXISTS)) {
		g_test_skip("Missing test data");
		return;
	}

	/* load quirks so the canonical-ID and flags mappings are available */
	quirks_dir = g_test_build_filename(G_TEST_DIST, "tests", "quirks.d", NULL);
	fu_context_set_path(ctx, FU_PATH_KIND_DATADIR_QUIRKS, quirks_dir);
	fu_context_add_flag(ctx, FU_CONTEXT_FLAG_NO_CACHE);
	ret = fu_context_load(ctx, progress, FU_CONTEXT_LOAD_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);

	/* load BIOS settings from a Lenovo P620 (with thinklmi driver problems) */
	test_dir = g_build_filename(base_dir, "lenovo-p620", NULL);
	if (g_file_test(test_dir, G_FILE_TEST_EXISTS)) {
		fu_context_set_path(ctx, FU_PATH_KIND_SYSFSDIR_FW_ATTRIB, test_dir);
		ret = fu_context_reload_bios_settings(ctx, &error);
		g_assert_no_error(error);
		g_assert_true(ret);
	}
	g_free(test_dir);

	/* load BIOS settings from a Lenovo P620 running 6.3 */
	test_dir = g_build_filename(base_dir, "lenovo-p620-6.3", NULL);
	if (g_file_test(test_dir, G_FILE_TEST_EXISTS)) {
		fu_context_set_path(ctx, FU_PATH_KIND_SYSFSDIR_FW_ATTRIB, test_dir);
		ret = fu_context_reload_bios_settings(ctx, &error);
		g_assert_no_error(error);
		g_assert_true(ret);

		p620_6_3_settings = fu_context_get_bios_settings(ctx);
		p620_6_3_items = fu_bios_settings_get_all(p620_6_3_settings);
		g_assert_cmpint(p620_6_3_items->len, ==, 5);

		/* make sure nothing pending */
		ret = fu_context_get_bios_setting_pending_reboot(ctx);
		g_assert_false(ret);

		/* check a BIOS setting reads from kernel 6.3 as expected by fwupd */
		setting = fu_context_get_bios_setting(ctx, "com.thinklmi.AMDMemoryGuard");
		g_assert_nonnull(setting);
		tmp = fwupd_bios_setting_get_name(setting);
		g_assert_cmpstr(tmp, ==, "AMDMemoryGuard");
		tmp = fwupd_bios_setting_get_description(setting);
		g_assert_cmpstr(tmp, ==, "AMDMemoryGuard");
		g_assert_cmpstr(fwupd_bios_setting_get_help_text(setting),
				==,
				"Synthetic option help for metadata testing.\nSecond line.");
		g_assert_cmpstr(fwupd_bios_setting_get_display_group(setting),
				==,
				"Main / CPU/SoC / Memory");
		tmp = fwupd_bios_setting_get_current_value(setting);
		g_assert_cmpstr(tmp, ==, "Disable");
		values = fwupd_bios_setting_get_possible_values(setting);
		for (guint i = 0; i < values->len; i++) {
			const gchar *possible = g_ptr_array_index(values, i);
			if (i == 0)
				g_assert_cmpstr(possible, ==, "Disable");
			if (i == 1)
				g_assert_cmpstr(possible, ==, "Enable");
		}

		/* try to read an BIOS setting known to have ][Status] to make sure we worked
		 * around the thinklmi bug sufficiently
		 */
		setting = fu_context_get_bios_setting(ctx, "com.thinklmi.StartupSequence");
		g_assert_nonnull(setting);
		tmp = fwupd_bios_setting_get_current_value(setting);
		g_assert_cmpstr(tmp, ==, "Primary");
		g_assert_null(fwupd_bios_setting_get_help_text(setting));
		g_assert_null(fwupd_bios_setting_get_display_group(setting));
		values = fwupd_bios_setting_get_possible_values(setting);
		for (guint i = 0; i < values->len; i++) {
			const gchar *possible = g_ptr_array_index(values, i);
			if (i == 0)
				g_assert_cmpstr(possible, ==, "Primary");
			if (i == 1)
				g_assert_cmpstr(possible, ==, "Automatic");
		}

		/* check BIOS settings that should be read only */
		for (guint i = 0; i < p620_6_3_items->len; i++) {
			const gchar *name;
			gboolean ro;

			setting = g_ptr_array_index(p620_6_3_items, i);
			ro = fwupd_bios_setting_get_read_only(setting);
			tmp = fwupd_bios_setting_get_current_value(setting);
			name = fwupd_bios_setting_get_name(setting);
			g_debug("%s: %s", name, tmp);
			if ((g_strcmp0(name, "pending_reboot") == 0) ||
			    (g_strrstr(tmp, "[Status") != NULL))
				g_assert_true(ro);
			else
				g_assert_false(ro);
		}
	}
	g_free(test_dir);

	/* load BIOS settings from a Lenovo P14s Gen1 */
	test_dir = g_build_filename(base_dir, "lenovo-p14s-gen1", NULL);
	if (g_file_test(test_dir, G_FILE_TEST_EXISTS)) {
		fu_context_set_path(ctx, FU_PATH_KIND_SYSFSDIR_FW_ATTRIB, test_dir);
		ret = fu_context_reload_bios_settings(ctx, &error);
		g_assert_no_error(error);
		g_assert_true(ret);
		g_clear_error(&error);
	}
	g_free(test_dir);

	/* load BIOS settings from a Dell XPS 9310 */
	test_dir = g_build_filename(base_dir, "dell-xps13-9310", NULL);
	if (g_file_test(test_dir, G_FILE_TEST_EXISTS)) {
		fu_context_set_path(ctx, FU_PATH_KIND_SYSFSDIR_FW_ATTRIB, test_dir);
		ret = fu_context_reload_bios_settings(ctx, &error);
		g_assert_no_error(error);
		g_assert_true(ret);

		/* make sure that we DIDN'T parse reset_bios setting */
		setting = fu_context_get_bios_setting(ctx, FWUPD_BIOS_SETTING_RESET_BIOS);
		g_assert_null(setting);

		/* look at a integer BIOS setting */
		setting = fu_context_get_bios_setting(ctx, "com.dell-wmi-sysman.CustomChargeStop");
		g_assert_nonnull(setting);
		kind = fwupd_bios_setting_get_kind(setting);
		g_assert_cmpint(kind, ==, FWUPD_BIOS_SETTING_KIND_INTEGER);
		integer = fwupd_bios_setting_get_lower_bound(setting);
		g_assert_cmpint(integer, ==, 55);
		integer = fwupd_bios_setting_get_upper_bound(setting);
		g_assert_cmpint(integer, ==, 100);
		integer = fwupd_bios_setting_get_scalar_increment(setting);
		g_assert_cmpint(integer, ==, 1);

		/* look at a string BIOS setting */
		setting = fu_context_get_bios_setting(ctx, "com.dell-wmi-sysman.Asset");
		g_assert_nonnull(setting);
		integer = fwupd_bios_setting_get_lower_bound(setting);
		g_assert_cmpint(integer, ==, 1);
		integer = fwupd_bios_setting_get_upper_bound(setting);
		g_assert_cmpint(integer, ==, 64);
		tmp = fwupd_bios_setting_get_description(setting);
		g_assert_cmpstr(tmp, ==, "Asset Tag");

		/* look at a enumeration BIOS setting */
		setting = fu_context_get_bios_setting(ctx, "com.dell-wmi-sysman.BiosRcvrFrmHdd");
		g_assert_nonnull(setting);
		kind = fwupd_bios_setting_get_kind(setting);
		g_assert_cmpint(kind, ==, FWUPD_BIOS_SETTING_KIND_ENUMERATION);
		values = fwupd_bios_setting_get_possible_values(setting);
		for (guint i = 0; i < values->len; i++) {
			const gchar *possible = g_ptr_array_index(values, i);
			if (i == 0)
				g_assert_cmpstr(possible, ==, "Disabled");
			if (i == 1)
				g_assert_cmpstr(possible, ==, "Enabled");
		}

		/* make sure we defaulted UEFI Secure boot to read only if enabled */
		setting = fu_context_get_bios_setting(ctx, "com.dell-wmi-sysman.SecureBoot");
		g_assert_nonnull(setting);
		ret = fwupd_bios_setting_get_read_only(setting);
		g_assert_true(ret);

		/* the AppStream ID comes from the quirk mapping; the icon and name
		 * come from the abstract table in fu-bios-settings.c */
		tmp = fwupd_bios_setting_get_appstream_id(setting);
		g_assert_cmpstr(tmp, ==, "org.fwupd.bios.secure-boot");
		tmp = fwupd_bios_setting_get_icon(setting);
		g_assert_cmpstr(tmp, ==, "application-certificate");
		tmp = fwupd_bios_setting_get_description(setting);
		g_assert_cmpstr(tmp, ==, "Secure Boot");

		/* the same setting can be looked up by its AppStream ID */
		g_assert_true(fu_context_get_bios_setting(ctx, "org.fwupd.bios.secure-boot") ==
			      setting);

		/* a device-toggle setting also picks up its AppStream ID */
		setting = fu_context_get_bios_setting(ctx, "com.dell-wmi-sysman.Camera");
		g_assert_nonnull(setting);
		tmp = fwupd_bios_setting_get_appstream_id(setting);
		g_assert_cmpstr(tmp, ==, "org.fwupd.bios.camera.enabled");

		/* an esoteric setting has no AppStream ID */
		setting = fu_context_get_bios_setting(ctx, "com.dell-wmi-sysman.Asset");
		g_assert_nonnull(setting);
		tmp = fwupd_bios_setting_get_appstream_id(setting);
		g_assert_null(tmp);
	}
	g_free(test_dir);

	/* load BIOS settings from a HP Z2 Mini G1a */
	test_dir = g_build_filename(base_dir, "hp-z2-mini-g1a", NULL);
	if (g_file_test(test_dir, G_FILE_TEST_EXISTS)) {
		fu_context_set_path(ctx, FU_PATH_KIND_SYSFSDIR_FW_ATTRIB, test_dir);
		ret = fu_context_reload_bios_settings(ctx, &error);
		g_assert_no_error(error);
		g_assert_true(ret);
	}
}

/* make sure setup still works, and canonical IDs stay unset, when quirks are disabled */
static void
fu_bios_settings_no_quirks_func(void)
{
	gboolean ret;
	const gchar *tmp;
	FwupdBiosSetting *setting;
	g_autofree gchar *base_dir = NULL;
	g_autofree gchar *test_dir = NULL;
	g_autoptr(FuContext) ctx = fu_context_new_full(FU_CONTEXT_FLAG_NO_QUIRKS);
	g_autoptr(GError) error = NULL;

#ifdef _WIN32
	g_test_skip("BIOS settings not supported on Windows");
	return;
#endif

	base_dir = g_test_build_filename(G_TEST_DIST, "tests", "bios-attrs", NULL);
	test_dir = g_build_filename(base_dir, "dell-xps13-9310", NULL);
	if (!g_file_test(test_dir, G_FILE_TEST_EXISTS)) {
		g_test_skip("Missing test data");
		return;
	}

	fu_context_set_path(ctx, FU_PATH_KIND_SYSFSDIR_FW_ATTRIB, test_dir);
	ret = fu_context_reload_bios_settings(ctx, &error);
	g_assert_no_error(error);
	g_assert_true(ret);

	/* the setting still loads, but has no AppStream ID */
	setting = fu_context_get_bios_setting(ctx, "com.dell-wmi-sysman.SecureBoot");
	g_assert_nonnull(setting);
	tmp = fwupd_bios_setting_get_appstream_id(setting);
	g_assert_null(tmp);
}

static void
fu_bios_settings_metadata_bounds_func(void)
{
	g_autoptr(FuContext) ctx = fu_context_new_full(FU_CONTEXT_FLAG_NO_QUIRKS);
	g_autoptr(GError) error = NULL;
	g_autofree gchar *test_dir =
	    g_test_build_filename(G_TEST_BUILT, "bios-metadata-XXXXXX", NULL);
	g_autofree gchar *source = g_test_build_filename(G_TEST_DIST,
							 "tests",
							 "bios-attrs",
							 "lenovo-p620-6.3",
							 "thinklmi",
							 "attributes",
							 "AMDMemoryGuard",
							 NULL);
	g_autofree gchar *folder = NULL;
	const gchar *keys[] = {"type", "current_value", "possible_values", "display_name"};
	const gchar *metadata_keys[] = {"help_text", "display_group"};
	const gsize sizes[] = {0, 65536, 65537, 1, 3};

#ifdef _WIN32
	g_test_skip("BIOS settings not supported on Windows");
	return;
#endif
	if (!g_file_test(source, G_FILE_TEST_IS_DIR)) {
		g_test_skip("Missing test data");
		return;
	}
	g_assert_nonnull(g_mkdtemp(test_dir));
	folder = g_build_filename(test_dir, "thinklmi", "attributes", "Test", NULL);
	g_assert_cmpint(g_mkdir_with_parents(folder, 0700), ==, 0);
	for (guint i = 0; i < G_N_ELEMENTS(keys); i++) {
		g_autofree gchar *src_path = g_build_filename(source, keys[i], NULL);
		g_autofree gchar *dst_path = g_build_filename(folder, keys[i], NULL);
		g_autoptr(GFile) src = g_file_new_for_path(src_path);
		g_autoptr(GFile) dst = g_file_new_for_path(dst_path);
		g_assert_true(g_file_copy(src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &error));
		g_assert_no_error(error);
	}
	fu_context_set_path(ctx, FU_PATH_KIND_SYSFSDIR_FW_ATTRIB, test_dir);
	for (guint i = 0; i < G_N_ELEMENTS(sizes); i++) {
		FwupdBiosSetting *setting;
		g_autofree gchar *text = g_strnfill(sizes[i], 'x');
		if (i == 3)
			text[0] = (gchar)0xff;
		if (i == 4)
			text[1] = '\0';
		for (guint j = 0; j < G_N_ELEMENTS(metadata_keys); j++) {
			g_autofree gchar *path = g_build_filename(folder, metadata_keys[j], NULL);
			g_assert_true(g_file_set_contents(path, text, sizes[i], &error));
			g_assert_no_error(error);
		}
		g_assert_true(fu_context_reload_bios_settings(ctx, &error));
		g_assert_no_error(error);
		setting = fu_context_get_bios_setting(ctx, "com.thinklmi.Test");
		g_assert_nonnull(setting);
		if (sizes[i] == 65536) {
			g_assert_cmpstr(fwupd_bios_setting_get_help_text(setting), ==, text);
			g_assert_cmpstr(fwupd_bios_setting_get_display_group(setting), ==, text);
		} else {
			g_assert_null(fwupd_bios_setting_get_help_text(setting));
			g_assert_null(fwupd_bios_setting_get_display_group(setting));
		}
	}
	g_assert_true(fu_path_rmtree(test_dir, &error));
	g_assert_no_error(error);
}

int
main(int argc, char **argv)
{
	(void)g_setenv("G_TEST_SRCDIR", SRCDIR, FALSE);
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/fwupd/bios-settings/metadata-bounds",
			fu_bios_settings_metadata_bounds_func);
	g_test_add_func("/fwupd/bios-settings/load", fu_bios_settings_load_func);
	g_test_add_func("/fwupd/bios-settings/no-quirks", fu_bios_settings_no_quirks_func);
	return g_test_run();
}
