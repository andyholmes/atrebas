// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

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
test_bookmarks_basic (void)
{
  g_autoptr (GeocodeBackend) backend = NULL;
  g_autoptr (GListModel) bookmarks = NULL;
  g_autoptr (AtrebasFeature) feature = NULL;
  g_autoptr (GeocodePlace) place = NULL;
  g_autoptr (GMainLoop) loop = NULL;
  gboolean has_place = FALSE;

  backend = test_get_backend ();
  feature = test_get_feature ();
  loop = g_main_loop_new (NULL, FALSE);

  bookmarks = atrebas_bookmarks_get_default ();
  g_assert_true (ATREBAS_IS_BOOKMARKS (bookmarks));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Addition */
  has_place = atrebas_bookmarks_has_place (ATREBAS_BOOKMARKS (bookmarks),
                                       GEOCODE_PLACE (feature));
  g_assert_false (has_place);

  /* Addition */
  atrebas_bookmarks_add_place (ATREBAS_BOOKMARKS (bookmarks),
                           GEOCODE_PLACE (feature));

  has_place = atrebas_bookmarks_has_place (ATREBAS_BOOKMARKS (bookmarks),
                                       GEOCODE_PLACE (feature));
  g_assert_true (has_place);

  /* Removal */
  atrebas_bookmarks_remove_place (ATREBAS_BOOKMARKS (bookmarks),
                              GEOCODE_PLACE (feature));

  has_place = atrebas_bookmarks_has_place (ATREBAS_BOOKMARKS (bookmarks),
                                       GEOCODE_PLACE (feature));
  g_assert_false (has_place);


  /* Re-add for load test */
  atrebas_bookmarks_add_place (ATREBAS_BOOKMARKS (bookmarks),
                           GEOCODE_PLACE (feature));

  has_place = atrebas_bookmarks_has_place (ATREBAS_BOOKMARKS (bookmarks),
                                       GEOCODE_PLACE (feature));
  g_assert_true (has_place);

  /* Free the bookmarks manager to force a reload */
  g_clear_object (&bookmarks);
  bookmarks = atrebas_bookmarks_get_default ();
  g_assert_true (ATREBAS_IS_BOOKMARKS (bookmarks));

  g_signal_connect (bookmarks,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    loop);
  g_main_loop_run (loop);

  has_place = atrebas_bookmarks_has_place (ATREBAS_BOOKMARKS (bookmarks),
                                       GEOCODE_PLACE (feature));
  g_assert_true (has_place);

  /* Thoroughly check the GListModel implementation */
  g_assert_true (g_list_model_get_item_type (bookmarks) == GEOCODE_TYPE_PLACE);
  g_assert_cmpuint (g_list_model_get_n_items (bookmarks), ==, 1);

  place = g_list_model_get_item (bookmarks, 0);
  g_assert_cmpstr (geocode_place_get_name (place), ==, ATREBAS_TEST_FEATURE_NAME);
  g_clear_object (&place);

  place = g_list_model_get_item (bookmarks, 1);
  g_assert_null (place);

  place = g_list_model_get_item (bookmarks, 1);
  g_assert_null (place);

  place = g_list_model_get_item (bookmarks, 0);
  g_assert_cmpstr (geocode_place_get_name (place), ==, ATREBAS_TEST_FEATURE_NAME);
  g_clear_object (&place);

  place = g_list_model_get_item (bookmarks, 2);
  g_assert_null (place);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/atrebas/bookmarks",
                   test_bookmarks_basic);

  return g_test_run ();
}

