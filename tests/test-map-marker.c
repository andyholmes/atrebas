// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>

#include "mock-common.h"


static void
test_map_marker_basic (void)
{
  GtkWidget *widget;
  GtkWidget *window;
  g_autoptr (AtrebasFeature) feature = NULL;
  g_autoptr (GListModel) features = NULL;
  g_autoptr (GeocodePlace) place_out = NULL;
  g_autofree char *icon_out = NULL;
  int icon_size = 0;

  feature = test_get_feature ();
  widget = atrebas_map_marker_new (GEOCODE_PLACE (feature));
  g_assert_true (ATREBAS_IS_MAP_MARKER (widget));

  /* Realize the widget */
  window = gtk_window_new ();
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (widget,
                "features",  &features,
                "place",     &place_out,
                "icon-name", &icon_out,
                "icon-size", &icon_size,
                NULL);
  g_assert_true (G_IS_LIST_MODEL (features));
  g_assert_true (G_IS_LIST_MODEL (atrebas_map_marker_get_features (ATREBAS_MAP_MARKER (widget))));
  g_assert_true (atrebas_feature_equal (feature, place_out));
  g_assert_true (atrebas_feature_equal (feature, atrebas_map_marker_get_place (ATREBAS_MAP_MARKER (widget))));
  g_assert_cmpstr ("atrebas-location-symbolic", ==, icon_out);
  g_assert_cmpstr ("atrebas-location-symbolic", ==, atrebas_map_marker_get_icon_name (ATREBAS_MAP_MARKER (widget)));
  g_assert_cmpint (16, ==, icon_size);
  g_assert_cmpint (16, ==, atrebas_map_marker_get_icon_size (ATREBAS_MAP_MARKER (widget)));
  g_clear_object (&place_out);

  g_object_set (widget,
                "place", NULL,
                NULL);
  g_object_get (widget,
                "place", &place_out,
                NULL);
  g_assert_null (place_out);
  g_assert_null (atrebas_map_marker_get_place (ATREBAS_MAP_MARKER (widget)));

  /* Wait for the window to close */
  gtk_window_destroy (GTK_WINDOW (window));

  while (window != NULL)
    g_main_context_iteration (NULL, FALSE);
}

int
main (int argc,
     char *argv[])
{
  test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/atrebas/map-marker",
                   test_map_marker_basic);

  return g_test_run ();
}

