// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>

#include "mock-common.h"


static void
test_preferences_window_basic (void)
{
  GtkWidget *window = NULL;

  window = g_object_new (ATREBAS_TYPE_PREFERENCES_WINDOW, NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  /* Wait for the window to present */
  gtk_window_present (GTK_WINDOW (window));

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

  g_test_add_func ("/atrebas/preferences-window",
                   test_preferences_window_basic);

  return g_test_run ();
}

