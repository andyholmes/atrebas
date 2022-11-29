# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

project('atrebas', 'c',
          license: 'GPL-2.0-or-later',
          version: '0.1.0',
    meson_version: '>= 0.59.0',
  default_options: [
    'buildtype=debugoptimized',
    'warning_level=2',
    'werror=true',
  ],
)

cc = meson.get_compiler('c')
i18n = import('i18n')
gnome = import('gnome')


#
# Versioning
#
atrebas_version = meson.project_version()
version_split = atrebas_version.split('.')
MAJOR_VERSION = version_split[0].to_int()
MINOR_VERSION = version_split[1].to_int()
MICRO_VERSION = version_split[2].to_int()

atrebas_api_version = '@0@'.format(atrebas_version.split('.')[0].to_int())
atrebas_api_name = 'atrebas-@0@'.format(atrebas_api_version)

#
# Build Profile
#
profile = get_option('profile')

if profile == 'devel'
  application_id = 'ca.andyholmes.Atrebas.Devel'
  application_path = '/ca/andyholmes/Atrebas/Devel'
  profile_suffix = ' (Devel)'
else
  application_id = 'ca.andyholmes.Atrebas'
  application_path = '/ca/andyholmes/Atrebas'
  profile_suffix = ''
endif


#
# Paths
#
prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
includedir = join_paths(prefix, get_option('includedir'))
libdir = join_paths(prefix, get_option('libdir'))
localedir = join_paths(prefix, get_option('localedir'))

pkgdatadir = join_paths(datadir, meson.project_name())
pkgincludedir = join_paths(includedir, meson.project_name())
pkglibdir = join_paths(libdir, meson.project_name())
podir = join_paths(meson.project_source_root(), 'po')


#
# Compiler
#
cc = meson.get_compiler('c')

release_args = []
project_c_args = [
  '-DATREBAS_COMPILATION',
  
  '-Wformat=2',
  '-Wincompatible-pointer-types',
  '-Wint-conversion',
  '-Wint-to-pointer-cast',
  '-Wmissing-include-dirs',
  '-Woverflow',
  '-Wpointer-arith',
  '-Wpointer-to-int-cast',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wstrict-prototypes',
  '-Wundef',

  '-Wno-discarded-array-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',

  # These should be kept in sync (eg. GTK 4.4 requires GLib 2.66)
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_66',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_66',
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_6',

  # TODO: These should be fixed instead of downgraded to warnings
  '-Wno-error=unused-function',
]
project_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]

if get_option('buildtype') != 'plain'
  project_c_args += ['-fstack-protector-strong']
endif

if get_option('debug')
  project_c_args += ['-fno-omit-frame-pointer']
endif

if get_option('optimization') in ['2', '3', 's']
  project_c_args += ['-DG_DISABLE_CAST_CHECKS']
  project_link_args += ['-Wl,-Bsymbolic']

  if not get_option('tests')
    release_args += ['-DG_DISABLE_ASSERT']
  endif
endif

if get_option('default_library') != 'static'
  project_c_args += ['-fvisibility=hidden']
endif

add_project_arguments(cc.get_supported_arguments(project_c_args),
  language: 'c',
)

add_project_link_arguments(cc.get_supported_link_arguments(project_link_args),
  language: 'c',
)


#
# config.h
#
config_h = configuration_data()
config_h_inc = include_directories('.')
config_h_defines = {
  # Desktop
  'APPLICATION_ID':     application_id,

  # AC_INIT
  'PACKAGE_NAME':       meson.project_name(),
  'PACKAGE_TARNAME':    '@0@-@1@'.format(meson.project_name(), meson.project_version()),
  'PACKAGE_VERSION':    meson.project_version(),
  'PACKAGE_STRING':     '@0@ @1@'.format(meson.project_name(), meson.project_version()),
  'PACKAGE_BUGREPORT':  'https://github.com/andyholmes/@0@/discussions'.format(meson.project_name()),
  'PACKAGE_URL':        'https://github.com/andyholmes/@0@'.format(meson.project_name()),
  
  'PACKAGE_DATADIR':    pkgdatadir,

  # i18n
  'GETTEXT_PACKAGE':    meson.project_name(),
  'LOCALEDIR':          localedir,
}

foreach define, value : config_h_defines
  config_h.set_quoted(define, value)
endforeach

configure_file(
         output: 'config.h',
  configuration: config_h,
)


#
# Dependencies
#
libm_dep = cc.find_library('m', required: true)
geocodeglib_dep = dependency('geocode-glib-2.0', version: '>= 3.26.0')
gio_dep = dependency('gio-2.0', version: '>= 2.44.0')
gtk_dep = dependency('gtk4', version: '>= 4.2.0')
jsonglib_dep = dependency('json-glib-1.0', version: '>= 1.6.0')
libadwaita_dep = dependency('libadwaita-1', version: '>= 1.2.0')
libgeoclue2_dep = dependency('libgeoclue-2.0', version: '>= 0.12.99')
libprotobuf_c_dep = dependency('libprotobuf-c', required: false)
libsoup_dep = dependency('libsoup-3.0', version: '>= 3.0')
sqlite_dep = dependency('sqlite3', version: '>= 3.24')

# Subprojects
libportal_dep = dependency('libportal',
          version: ['>= 0.5', '<= 0.6'],
         fallback: ['libportal', 'libportal_dep'],
  default_options: [
    'backends=gtk4',
    'docs=false',
    'introspection=false',
    'vapi=false',
    'werror=false',
  ],
)

libshumate_dep = dependency('shumate-1.0',
          version: '>= 1.0.0',
         fallback: ['libshumate', 'libshumate_dep'],
  default_options: [
    'gir=false',
    'gtk_doc=false',
    'vapi=false',
    'vector_renderer=@0@'.format(libprotobuf_c_dep.found()),
    'werror=false',
  ],
)


subdir('data')
subdir('po')
subdir('src')

if get_option('tests')
  subdir('tests')
endif

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)
