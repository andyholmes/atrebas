# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#
# Environment
#
test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'G_SLICE=always-malloc',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@'.format(
      join_paths(meson.current_build_dir(), 'schemas')),
  'GSETTINGS_SCHEMA_XML=@0@'.format(
      join_paths(meson.project_source_root(), 'data')),
  'MALLOC_CHECK_=2',
  # See: https://github.com/google/sanitizers/issues/1322
  'ASAN_OPTIONS=detect_leaks=1,intercept_tls_get_addr=0',
  'LSAN_OPTIONS=fast_unwind_on_malloc=0,suppressions=@0@'.format(
      join_paths(meson.current_source_dir(), 'extra', 'lsan.supp')),
  'TSAN_OPTIONS=force_seq_cst_atomics=1,history_size=5,suppressions=@0@'.format(
      join_paths(meson.current_source_dir(), 'extra', 'tsan.supp')),
  'UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1',
]

test_c_args = [
  '-DTEST_DATA_DIR="@0@"'.format(join_paths(meson.current_source_dir(), 'data')),
  '-DKANATA_TEST=1',
  '-I' + join_paths(meson.project_source_root(), 'src'),
]

test_link_args = [
]

#
# Mock
#
libatrebas_test_sources = [
  'mock-common.h',
  'mock-map-source.h',
  'mock-map-source.c',
]

libatrebas_test = static_library('libatrebas-test',
                                libatrebas_test_sources,
               c_args: test_c_args,
            link_args: test_link_args,
         dependencies: atrebas_deps,
  include_directories: [config_h_inc, include_directories('.')],
                  pic: true,
)


#
# Tests
#
atrebas_tests = [
  'test-backend',
  'test-feature',
  'test-search-model',

  'test-bookmarks',
  'test-feature-layer',
  'test-legend',
  'test-legend-row',
  'test-legend-symbol',
  'test-map-marker',
  'test-place-bar',
  'test-place-header',
  'test-preferences-window',
]

# Compile the GSettings schemas for the tests below
test_gsettings_schemas = custom_target('test-gsettings-schema',
            output: 'gschemas.compiled',
           command: find_program('gsettings-test-init.sh'),
               env: test_env,
  build_by_default: true,
           install: false,
)

foreach test : atrebas_tests
  source = ['@0@.c'.format(test)]

  test_program = executable(test, source,
            c_args: test_c_args,
         link_args: test_link_args,
        link_whole: [libatrebas, libatrebas_test],
      dependencies: libatrebas_dep,
    include_directories: [config_h_inc, include_directories('.')],
  )

  test(test,
       test_program,
            env: test_env,
        depends: test_gsettings_schemas,
    is_parallel: false,
          suite: 'atrebas',
  )
endforeach

