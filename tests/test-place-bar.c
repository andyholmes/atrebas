// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>

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
test_place_bar_basic (void)
{
  GtkWidget *widget;
  GtkWidget *window = NULL;
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (AtrebasFeature) feature = NULL;
  g_autoptr (GeocodePlace) place_out = NULL;
  GListModel *features = NULL;
  double latitude, longitude;

  g_test_log_set_fatal_handler (atrebas_test_mute_domain, "Adwaita");

  feature = test_get_feature ();
  loop = g_main_loop_new (NULL, FALSE);
  widget = atrebas_place_bar_new (GEOCODE_PLACE (feature));
  features = atrebas_place_bar_get_features (ATREBAS_PLACE_BAR (widget));

  g_assert_true (ATREBAS_IS_PLACE_BAR (widget));
  g_assert_true (G_IS_LIST_MODEL (features));

  g_signal_connect (features,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    loop);

  /* Realize the widget */
  window = gtk_window_new ();
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Check construct properties */
  g_object_get (widget,
                "place", &place_out,
                NULL);

  g_assert_true ((GeocodePlace *)feature == place_out);
  g_assert_true ((GeocodePlace *)feature == atrebas_place_bar_get_place (ATREBAS_PLACE_BAR (widget)));

  /* Set a location */
  /* TODO: GeocodeMockBackend */
  /* g_object_set (widget, */
  /*               "latitude",   42.0, */
  /*               "longitude", -120.0, */
  /*               NULL); */
  /* g_main_loop_run (loop); */

  g_object_get (widget,
                "latitude",  &latitude,
                "longitude", &longitude,
                NULL);
  /* g_assert_cmpfloat (latitude, ==, 42.0); */
  /* g_assert_cmpfloat (longitude, ==, -120.0); */
  g_assert_cmpfloat (latitude, ==, 23.2620255);
  g_assert_cmpfloat (longitude, ==, -103.158512);
  g_assert_cmpfloat (atrebas_place_bar_get_latitude (ATREBAS_PLACE_BAR (widget)), ==, 23.2620255);
  g_assert_cmpfloat (atrebas_place_bar_get_longitude (ATREBAS_PLACE_BAR (widget)), ==, -103.158512);

  /* GActions */
  gtk_widget_activate_action (widget, "place.bookmark", NULL);
  gtk_widget_activate_action (widget, "place.update", NULL);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

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

  g_test_add_func ("/atrebas/place-bar",
                   test_place_bar_basic);

  return g_test_run ();
}

