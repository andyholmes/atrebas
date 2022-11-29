// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>

#include "mock-common.h"


static void
test_legend_basic (void)
{
  GtkWidget *widget = NULL;
  GtkWidget *window = NULL;
  g_autoptr (AtrebasFeature) feature = NULL;
  g_autoptr (GListStore) layers = NULL;
  g_autoptr (GListStore) layers_out = NULL;

  feature = test_get_feature ();
  layers = g_list_store_new (SHUMATE_TYPE_LAYER);

  widget = atrebas_legend_new (G_LIST_MODEL (layers));
  g_assert_true (ATREBAS_IS_LEGEND (widget));

  /* Realize the widget */
  window = gtk_window_new ();
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (widget,
                "layers", &layers_out,
                NULL);
  g_assert_true (layers == layers_out);
  g_assert_true ((GListModel *)layers == atrebas_legend_get_layers (ATREBAS_LEGEND (widget)));

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

  g_test_add_func ("/atrebas/legend",
                   test_legend_basic);

  return g_test_run ();
}

