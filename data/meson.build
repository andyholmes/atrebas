# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

subdir('metainfo')


#
# Map Data
#
# This is a bundled copy of the GeoJSON data distributed by Native Lands Digital
#
install_subdir('maps',
      install_dir: pkgdatadir,
  strip_directory: false,
)


#
# Desktop File
#
desktop_conf = configuration_data()
desktop_conf.set('application_id', application_id)
desktop_file = i18n.merge_file(
        input: configure_file(
                  input: files('ca.andyholmes.Atrebas.desktop.in.in'),
                 output: 'ca.andyholmes.Atrebas.desktop.in',
          configuration: desktop_conf
        ),
       output: '@0@.desktop'.format(application_id),
      install: true,
  install_dir: join_paths(datadir, 'applications'),
       po_dir: podir,
         type: 'desktop'
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils,
       args: [desktop_file],
    depends: [desktop_file],
      suite: 'data',
  )
endif


#
# Application Icon
#
scalable_icondir = join_paths('icons', 'hicolor', 'scalable', 'apps')
install_data(
  join_paths(scalable_icondir, 'ca.andyholmes.Atrebas.svg'),
  install_dir: join_paths(datadir, scalable_icondir),
       rename: '@0@.svg'.format(application_id),
)

symbolic_icondir = join_paths('icons', 'hicolor', 'symbolic', 'apps')
install_data(
  join_paths(symbolic_icondir, 'ca.andyholmes.Atrebas-symbolic.svg'),
  install_dir: join_paths(datadir, symbolic_icondir),
       rename: '@0@-symbolic.svg'.format(application_id),
)


#
# GSettings
#
install_data('ca.andyholmes.Atrebas.gschema.xml',
  install_dir: join_paths(datadir, 'glib-2.0', 'schemas'),
)

glib_compile_schemas = find_program('glib-compile-schemas', required: false)
if glib_compile_schemas.found()
  test('Validate GSettings Schemas',
       glib_compile_schemas,
     args: ['--strict', '--dry-run', meson.current_source_dir()],
    suite: 'data',
  )
endif


#
# GResource
#
atrebas_resources = gnome.compile_resources(
  'atrebas-resources',
  'ca.andyholmes.Atrebas.gresource.xml'.format(application_id),
  c_name: 'atrebas',
)


#
# DBus Service
#
dbus_conf = configuration_data()
dbus_conf.set('application_id', application_id)
dbus_conf.set('bindir', bindir)
dbus_file = configure_file(
          input: 'ca.andyholmes.Atrebas.service.in',
         output: '@0@.service'.format(application_id),
  configuration: dbus_conf,
        install: true,
    install_dir: join_paths(datadir, 'dbus-1', 'services'),
)

