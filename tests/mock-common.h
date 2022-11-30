// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <locale.h>

#include "atrebas-backend.h"
#include "atrebas-feature.h"
#include "atrebas-utils.h"

#include "mock-map-source.h"

#define ATREBAS_TEST_FEATURE_NAME "Zacateco "
#define ATREBAS_TEST_FEATURE_ID  "8de8b7d2a79cc786dca0b574563b92c4"
#define ATREBAS_TEST_FEATURE_LAT    23.26
#define ATREBAS_TEST_FEATURE_LON  -103.16


static void
test_get_backend_cb (AtrebasBackend   *backend,
                     GAsyncResult *result,
                     GMainLoop    *loop)
{
  GError *error = NULL;

  if (!atrebas_backend_load_finish (backend, result, &error))
    g_assert_no_error (error);

  g_main_loop_quit (loop);
}

/**
 * test_get_backend:
 *
 * Get the default backend pre-loaded with test data.
 *
 * Returns: (transfer none): a #GeocodeBackend
 */
static inline GeocodeBackend *
(test_get_backend) (void)
{
  GeocodeBackend *backend = atrebas_backend_get_default ();
  g_autoptr (GMainLoop) loop = NULL;

  loop = g_main_loop_new (NULL, FALSE);
  atrebas_backend_load (ATREBAS_BACKEND (backend),
                        TEST_DATA_DIR"/testFeatureCollection.json",
                        ATREBAS_MAP_THEME_TERRITORY,
                        NULL,
                        (GAsyncReadyCallback)test_get_backend_cb,
                        loop);
  g_main_loop_run (loop);

  return backend;
}
#define test_get_backend() (test_get_backend())

/**
 * test_get_feature:
 *
 * Get a #AtrebasFeature loaded from test data.
 *
 * Returns: (transfer full): a #AtrebasFeature
 */
static inline AtrebasFeature *
test_get_feature (void)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (JsonNode) node = NULL;

  /* Load the JSON */
  parser = json_parser_new ();
  json_parser_load_from_file (parser, TEST_DATA_DIR"/testFeature.json", NULL);
  node = json_parser_steal_root (parser);

  return atrebas_feature_deserialize (node, NULL);
}

/**
 * atrebas_test_mute_domain:
 * @log_domain: the log domain of the message
 * @log_level: a #GLogLevel
 * @message: an error message
 * @user_data: the log domain to match against
 *
 * A #GTestLogFatalFunc for preventing fatal errors if @log_domain matches
 * @user_data with g_strcmp0().
 */
static inline gboolean
atrebas_test_mute_domain (const char     *log_domain,
                      GLogLevelFlags  log_level,
                      const char     *message,
                      gpointer        user_data)
{
  const char *match_domain = user_data;

  if (g_strcmp0 (log_domain, match_domain) == 0)
    return FALSE;

  return TRUE;
}

/**
 * test_ui_init:
 * @argcp: Address of the `argc` parameter of the
 *        main() function. Changed if any arguments were handled.
 * @argvp: (inout) (array length=argcp): Address of the
 *        `argv` parameter of main().
 *        Any parameters understood by g_test_init() or gtk_init() are
 *        stripped before return.
 * @...: currently unused
 *
 * This function is used to initialize a GUI test program for Valent.
 *
 * In order, it will:
 * - Call g_content_type_set_mime_dirs() to ensure GdkPixbuf works
 * - Call g_test_init() with the %G_TEST_OPTION_ISOLATE_DIRS option
 * - Call g_type_ensure() for public classes
 * - Set the locale to “en_US.UTF-8”
 * - Call gtk_init()
 * - Call adw_init()
 *
 * Like g_test_init(), any known arguments will be processed and stripped from
 * @argcp and @argvp.
 */
static inline void
test_ui_init (int    *argcp,
              char ***argvp,
              ...)
{
  g_content_type_set_mime_dirs (NULL);
  g_test_init (argcp, argvp, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_type_ensure (ATREBAS_TYPE_BACKEND);
  g_type_ensure (ATREBAS_TYPE_FEATURE);
  g_type_ensure (ATREBAS_TYPE_SEARCH_MODEL);
  g_type_ensure (ATREBAS_TYPE_BOOKMARKS);
  g_type_ensure (ATREBAS_TYPE_FEATURE_LAYER);
  g_type_ensure (ATREBAS_TYPE_LEGEND);
  g_type_ensure (ATREBAS_TYPE_LEGEND_ROW);
  g_type_ensure (ATREBAS_TYPE_LEGEND_SYMBOL);
  g_type_ensure (ATREBAS_TYPE_MAP_MARKER);
  g_type_ensure (ATREBAS_TYPE_MAP_VIEW);
  g_type_ensure (ATREBAS_TYPE_PLACE_BAR);
  g_type_ensure (ATREBAS_TYPE_PLACE_HEADER);
  g_type_ensure (ATREBAS_TYPE_PREFERENCES_WINDOW);
  g_type_ensure (ATREBAS_TYPE_WINDOW);

  gtk_disable_setlocale ();
  setlocale (LC_ALL, "en_US.UTF-8");
  gtk_init ();
  adw_init ();
}

