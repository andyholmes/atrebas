# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>


# Sources
atrebas_headers = files([
  'atrebas-macros.h',
  'atrebas-backend.h',
  'atrebas-feature.h',
  'atrebas-search-model.h',
  'atrebas-application.h',
  'atrebas-bookmarks.h',
  'atrebas-feature-layer.h',
  'atrebas-legend.h',
  'atrebas-legend-row.h',
  'atrebas-legend-symbol.h',
  'atrebas-map-marker.h',
  'atrebas-map-view.h',
  'atrebas-place-bar.h',
  'atrebas-place-header.h',
  'atrebas-preferences-window.h',
  'atrebas-utils.h',
  'atrebas-window.h',
])

atrebas_sources = files([
  'atrebas-backend.c',
  'atrebas-backend-utils.c',
  'atrebas-feature.c',
  'atrebas-search-model.c',
  'atrebas-application.c',
  'atrebas-bookmarks.c',
  'atrebas-feature-layer.c',
  'atrebas-legend.c',
  'atrebas-legend-row.c',
  'atrebas-legend-symbol.c',
  'atrebas-map-marker.c',
  'atrebas-map-view.c',
  'atrebas-place-bar.c',
  'atrebas-place-header.c',
  'atrebas-preferences-window.c',
  'atrebas-utils.c',
  'atrebas-window.c',
])

atrebas_enums = gnome.mkenums_simple('atrebas-enums',
  sources: atrebas_headers,
)


# Executable
atrebas_c_args = ['-Wno-missing-declarations']
atrebas_link_args = []
atrebas_deps = [
  libm_dep,
  gio_dep,
  gtk_dep,
  geocodeglib_dep,
  jsonglib_dep,
  libadwaita_dep,
  libgeoclue2_dep,
  libshumate_dep,
  libsoup_dep,
  sqlite_dep,
]

libatrebas = static_library('libatrebas-' + atrebas_api_version,
                           atrebas_sources,
                           atrebas_headers,
                           atrebas_enums,
                           atrebas_resources,
               c_args: atrebas_c_args + release_args + ['-DATREBAS_COMPILATION'],
            link_args: atrebas_link_args,
         dependencies: atrebas_deps,
  include_directories: [config_h_inc, include_directories('.')],
                  pic: true,
)

libatrebas_dep = declare_dependency(
            link_with: libatrebas,
         dependencies: atrebas_deps,
  include_directories: [include_directories('.')],
)

atrebas = executable('atrebas', 'main.c',
              install: true,
               c_args: atrebas_c_args + release_args,
            link_args: atrebas_link_args,
           link_whole: libatrebas,
         dependencies: libatrebas_dep,
  include_directories: [config_h_inc, include_directories('.')],
                  pie: true,
)

