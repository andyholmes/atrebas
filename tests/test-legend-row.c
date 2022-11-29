// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>

#include "mock-common.h"


static void
test_legend_row_basic (void)
{
  GtkWidget *widget;
  GtkWidget *list = NULL;
  GtkWidget *window = NULL;
  g_autoptr (AtrebasFeature) feature = NULL;
  GtkWidget *layer;
  g_autoptr (GtkWidget) layer_out = NULL;

  feature = test_get_feature ();
  layer = atrebas_feature_layer_new (NULL, feature);
  widget = atrebas_legend_row_new (SHUMATE_LAYER (layer));
  g_assert_true (ATREBAS_IS_LEGEND_ROW (widget));

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
  g_object_get (widget,
                "layer",   &layer_out,
                NULL);
  g_assert_true (layer == layer_out);
  g_assert_true ((ShumateLayer *)layer == atrebas_legend_row_get_layer (ATREBAS_LEGEND_ROW (widget)));

  /* GActions */
  gtk_widget_activate_action (widget, "row.bookmark", NULL);
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

  g_test_add_func ("/atrebas/legend-row",
                   test_legend_row_basic);

  return g_test_run ();
}

