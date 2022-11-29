# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

metainfo_conf = configuration_data()
metainfo_conf.set('application_id', application_id)
metainfo_conf.set('gettext_package', meson.project_name())
metainfo_file = i18n.merge_file(
        input: configure_file(
	          input: files('ca.andyholmes.Atrebas.metainfo.xml.in.in'),
	         output: 'ca.andyholmes.Atrebas.metainfo.xml.in',
          configuration: metainfo_conf,
        ),
       output: '@0@.metainfo.xml'.format(application_id),
      install: true,
  install_dir: join_paths(datadir, 'metainfo'),
       po_dir: podir,
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('Validate AppStream file', appstream_util,
       args: ['validate-relax', metainfo_file],
    depends: [metainfo_file],
      suite: 'data',
  )
endif


