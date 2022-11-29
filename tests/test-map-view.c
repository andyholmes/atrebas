// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>

#include "mock-common.h"


static void
test_map_view_basic (void)
{
  GtkWidget *widget;
  GtkWidget *list = NULL;
  GtkWidget *window = NULL;
  g_autoptr (AtrebasFeature) feature = NULL;
  g_autoptr (AtrebasFeature) feature_out = NULL;

  feature = test_get_feature ();
  widget = atrebas_map_view_new ();
  g_assert_true (ATREBAS_IS_MAP_VIEW (widget));

  /* Realize the widget */
  list = gtk_list_box_new ();
  gtk_list_box_append (GTK_LIST_BOX (list), widget);

  window = gtk_window_new ();
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_set_child (GTK_WINDOW (window), list);
  gtk_window_present (GTK_WINDOW (window));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  /* g_object_get (widget, */
  /*               "feature", &feature_out, */
  /*               NULL); */
  /* g_assert_true (atrebas_feature_equal (feature, feature_out)); */
  /* g_assert_true (atrebas_feature_equal (feature, atrebas_map_view_get_feature (ATREBAS_MAP_VIEW (widget)))); */
  /* g_clear_object (&feature_out); */

  /* g_object_set (widget, */
  /*               "feature", NULL, */
  /*               NULL); */
  /* g_object_get (widget, */
  /*               "feature", &feature_out, */
  /*               NULL); */
  /* g_assert_null (feature_out); */
  /* g_assert_null (atrebas_map_view_get_feature (ATREBAS_MAP_VIEW (widget))); */

  /* GActions */
  gtk_widget_activate_action (widget, "row.bookmark", NULL);
  gtk_widget_activate_action (widget, "row.center", NULL);
  gtk_widget_activate_action (widget, "row.info", NULL);

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

  g_test_add_func ("/atrebas/map-view",
                   test_map_view_basic);

  return g_test_run ();
}

