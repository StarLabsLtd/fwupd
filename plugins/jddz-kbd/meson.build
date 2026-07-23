cargs = ['-DG_LOG_DOMAIN="FuPluginJddzKbd"']

plugins += {meson.current_source_dir().split('/')[-1]: true}
plugin_quirks += files('jddz-kbd.quirk')
plugin_builtin_jddz_kbd = static_library(
  'fu_plugin_jddz_kbd',
  sources: [
    'fu-jddz-kbd-common.c',
    'fu-jddz-kbd-device.c',
    'fu-jddz-kbd-firmware.c',
    'fu-jddz-kbd-iap-device.c',
    'fu-jddz-kbd-plugin.c',
  ],
  include_directories: plugin_incdirs,
  link_with: plugin_libs,
  c_args: cargs,
  dependencies: plugin_deps,
)
plugin_builtins += plugin_builtin_jddz_kbd

if get_option('tests')
  e = executable(
    'jddz-kbd-self-test',
    sources: [
      'fu-self-test.c',
    ],
    include_directories: plugin_incdirs,
    dependencies: plugin_deps,
    link_with: [
      plugin_libs,
      plugin_builtin_jddz_kbd,
    ],
    install: true,
    install_rpath: libdir_pkg,
    install_tag: 'tests',
    install_dir: installed_test_bindir,
    c_args: [
      cargs,
      '-DSRCDIR="' + meson.current_source_dir() + '"',
    ],
  )
  test(
    'jddz-kbd-self-test',
    e,
    env: {
      'G_TEST_BUILDDIR': meson.current_build_dir(),
      'G_TEST_SRCDIR': meson.current_source_dir(),
    },
  )
endif
