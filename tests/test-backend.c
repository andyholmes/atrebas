// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>

#include "mock-common.h"


/* A simple pair of macros to avoid using GMainLoop */
static gboolean done = FALSE;

#define task_done                                       \
  done = TRUE;

#define task_wait                                       \
  while (!done) g_main_context_iteration (NULL, FALSE); \
  done = FALSE;


static void
load_cb (AtrebasBackend   *backend,
         GAsyncResult *result,
         gpointer      user_data)
{
  GError *error = NULL;

  if (!atrebas_backend_load_finish (backend, result, &error))
    g_assert_no_error (error);

  task_done;
}

static void
forward_search_cb (GeocodeBackend *backend,
                   GAsyncResult   *result,
                   gpointer        user_data)
{
  g_autolist (GeocodePlace) results = NULL;
  GError *error = NULL;

  results = geocode_backend_forward_search_finish (backend, result, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (results), ==, 2);

  task_done;
}

static void
reverse_resolve_cb (GeocodeBackend *backend,
                    GAsyncResult   *result,
                    gpointer        user_data)
{
  g_autolist (GeocodePlace) results = NULL;
  GError *error = NULL;

  results = geocode_backend_reverse_resolve_finish (backend, result, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (results), ==, 2);

  task_done;
}

static void
lookup_cb (AtrebasBackend   *backend,
           GAsyncResult *result,
           gpointer      user_data)
{
  g_autoptr (AtrebasFeature) feature = NULL;
  GError *error = NULL;

  feature = atrebas_backend_lookup_finish (backend, result, &error);
  g_assert_no_error (error);
  g_assert_true (ATREBAS_IS_FEATURE (feature));
  g_assert_cmpstr (ATREBAS_TEST_FEATURE_NAME, ==, geocode_place_get_name (GEOCODE_PLACE (feature)));

  task_done;
}


static void
test_backend_new (void)
{
  GeocodeBackend *backend = NULL;
  g_autofree char *path = NULL;

  backend = atrebas_backend_new (":memory:");

  g_assert_cmpstr (atrebas_backend_get_path (ATREBAS_BACKEND (backend)), ==, ":memory:");

  g_object_get (backend, "path", &path, NULL);
  g_assert_cmpstr (path, ==, ":memory:");

  /* Cleanup */
  g_clear_object (&backend);

  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

static void
test_backend_load (void)
{
  GeocodeBackend *backend = atrebas_backend_get_default ();

  atrebas_backend_load (ATREBAS_BACKEND (backend),
                        TEST_DATA_DIR"/testFeatureCollection.json",
                        ATREBAS_MAP_THEME_TERRITORY,
                        NULL,
                        (GAsyncReadyCallback)load_cb,
                        NULL);
  task_wait;
}

static inline GValue *
parameter_boolean (gboolean value)
{
  GValue *ret;

  ret = g_new0 (GValue, 1);
  g_value_init (ret, G_TYPE_BOOLEAN);
  g_value_set_boolean (ret, value);

  return ret;
}

static inline GValue *
parameter_string (const char *value)
{
  GValue *ret;

  ret = g_new0 (GValue, 1);
  g_value_init (ret, G_TYPE_STRING);
  g_value_set_string (ret, value);

  return ret;
}

static void
test_backend_operations (void)
{
  GeocodeBackend *backend = atrebas_backend_get_default ();
  g_autoptr (GHashTable) bounded_params = NULL;
  g_autoptr (GHashTable) forward_params = NULL;
  g_autoptr (GHashTable) reverse_params = NULL;
  g_autolist (GeocodePlace) forward_results = NULL;
  g_autolist (GeocodePlace) reverse_results = NULL;
  GError *error = NULL;

  atrebas_backend_load (ATREBAS_BACKEND (backend),
                        TEST_DATA_DIR"/testFeatureCollection.json",
                        ATREBAS_MAP_THEME_TERRITORY,
                        NULL,
                        (GAsyncReadyCallback)load_cb,
                        NULL);
  task_wait;

  /* Geocode Parameters */
  bounded_params = atrebas_geocode_parameters_for_location ("Zacateco");
  g_hash_table_insert (bounded_params,
                       (gpointer)"bounded",
                       parameter_boolean (TRUE));
  g_hash_table_insert (bounded_params,
                       (gpointer)"viewbox",
                       parameter_string ("-102.56,22.78,-102.57,22.77"));
  forward_params = atrebas_geocode_parameters_for_location ("Zacateco");
  reverse_params = atrebas_geocode_parameters_for_coordinates (22.78, -102.56);

  /* GeocodeBackend (async) */
  geocode_backend_forward_search_async (backend,
                                        bounded_params,
                                        NULL,
                                        (GAsyncReadyCallback)forward_search_cb,
                                        NULL);
  task_wait;

  geocode_backend_forward_search_async (backend,
                                        forward_params,
                                        NULL,
                                        (GAsyncReadyCallback)forward_search_cb,
                                        NULL);
  task_wait;

  geocode_backend_reverse_resolve_async (backend,
                                         reverse_params,
                                         NULL,
                                         (GAsyncReadyCallback)reverse_resolve_cb,
                                         NULL);
  task_wait;

  /* GeocodeBackend (sync) */
  forward_results = geocode_backend_forward_search (backend,
                                                    forward_params,
                                                    NULL,
                                                    &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (forward_results), ==, 2);

  reverse_results = geocode_backend_reverse_resolve (backend,
                                                     reverse_params,
                                                     NULL,
                                                     &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (reverse_results), ==, 2);

  /* Custom operations */
  atrebas_backend_lookup (ATREBAS_BACKEND (backend),
                          "1a06d1f9693a307ce18e674a7fb94d59",
                          NULL,
                          (GAsyncReadyCallback)lookup_cb,
                          NULL);
  task_wait;
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/atrebas/backend/new",
                   test_backend_new);
  g_test_add_func ("/atrebas/backend/load",
                   test_backend_load);
  g_test_add_func ("/atrebas/backend/operations",
                   test_backend_operations);

  return g_test_run ();
}

