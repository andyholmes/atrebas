// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>
#include <shumate/shumate.h>

#include "mock-common.h"


static void
test_feature_layer_basic (void)
{
  GtkWidget *widget;
  GtkWidget *window = NULL;
  ShumateViewport *viewport = NULL;
  g_autoptr (ShumateMapSource) source = NULL;
  g_autoptr (AtrebasFeature) feature = NULL;
  g_autoptr (AtrebasFeature) feature_out = NULL;

  viewport = shumate_viewport_new ();
  source = mock_map_source_new ();
  shumate_viewport_set_reference_map_source (viewport, source);
  feature = test_get_feature ();
  widget = atrebas_feature_layer_new (viewport, feature);
  g_assert_true (ATREBAS_IS_FEATURE_LAYER (widget));

  /* Realize the widget */
  window = gtk_window_new ();
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (widget,
                "feature", &feature_out,
                NULL);
  g_assert_true (atrebas_feature_equal (feature, feature_out));
  g_assert_true (atrebas_feature_equal (feature, atrebas_feature_layer_get_feature (ATREBAS_FEATURE_LAYER (widget))));
  g_clear_object (&feature_out);

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

  g_test_add_func ("/atrebas/feature-layer",
                   test_feature_layer_basic);

  return g_test_run ();
}

