// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>


#include "mock-common.h"


static void
on_items_changed (GListModel   *model,
                  unsigned int  position,
                  unsigned int  removed,
                  unsigned int  added,
                  GMainLoop    *loop)
{
  g_main_loop_quit (loop);
}

static void
test_search_model_basic (void)
{
  GListModel *model = NULL;
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (GeocodeBackend) backend = NULL;
  g_autoptr (AtrebasFeature) feature = NULL;
  g_autofree char *query = NULL;

  loop = g_main_loop_new (NULL, FALSE);
  backend = test_get_backend ();

  /* Create results object */
  model = atrebas_search_model_new (backend);
  backend = NULL;

  /* Wait for results */
  atrebas_search_model_set_query (ATREBAS_SEARCH_MODEL (model), "zacateco");
  g_signal_connect (model,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    loop);

  g_object_get (model,
                "backend", &backend,
                "query",   &query,
                NULL);

  g_assert_true (backend == atrebas_backend_get_default ());
  g_assert_true (atrebas_search_model_get_backend (ATREBAS_SEARCH_MODEL (model)) == backend);
  g_assert_cmpstr (query, ==, "zacateco");
  g_assert_cmpstr (atrebas_search_model_get_query (ATREBAS_SEARCH_MODEL (model)), ==, query);

  /* Expect 2 results */
  g_main_loop_run (loop);
  g_assert_true (g_list_model_get_item_type (model) == GEOCODE_TYPE_PLACE);
  g_assert_cmpuint (g_list_model_get_n_items (model), ==, 2);

  /* Thoroughly check the GListModel implementation */
  feature = g_list_model_get_item (model, 0);
  g_assert_cmpstr (atrebas_feature_get_name_fr (feature), ==, ATREBAS_TEST_FEATURE_NAME);
  g_clear_object (&feature);

  feature = g_list_model_get_item (model, 1);
  g_assert_cmpstr (atrebas_feature_get_name_fr (feature), ==, ATREBAS_TEST_FEATURE_NAME);
  g_clear_object (&feature);

  feature = g_list_model_get_item (model, 1);
  g_assert_cmpstr (atrebas_feature_get_name_fr (feature), ==, ATREBAS_TEST_FEATURE_NAME);
  g_clear_object (&feature);

  feature = g_list_model_get_item (model, 0);
  g_assert_cmpstr (atrebas_feature_get_name_fr (feature), ==, ATREBAS_TEST_FEATURE_NAME);
  g_clear_object (&feature);

  feature = g_list_model_get_item (model, 2);
  g_assert_null (feature);

  /* Expect no results */
  atrebas_search_model_set_query (ATREBAS_SEARCH_MODEL (model), "");
  g_main_loop_run (loop);
  g_assert_cmpuint (g_list_model_get_n_items (model), ==, 0);

  /* Cleanup */
  g_signal_handlers_disconnect_by_data (model, loop);
  g_assert_finalize_object (model);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/atrebas/search-model/basic",
                   test_search_model_basic);

  return g_test_run ();
}

