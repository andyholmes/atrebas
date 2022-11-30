// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-preferences-window"

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>

#include "atrebas-preferences-window.h"


struct _AtrebasPreferencesWindow
{
  AdwPreferencesWindow  parent_instance;
  GSettings            *settings;

  /* Template widgets */
  GtkWidget            *general_page;
  GtkWidget            *background_switch;
  AdwExpanderRow       *location_row;
  GtkWidget            *notification_switch;
};

G_DEFINE_TYPE (AtrebasPreferencesWindow, atrebas_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)


/*
 * GObject
 */
static void
atrebas_preferences_window_finalize (GObject *object)
{
  AtrebasPreferencesWindow *self = ATREBAS_PREFERENCES_WINDOW (object);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (atrebas_preferences_window_parent_class)->finalize (object);
}

static void
atrebas_preferences_window_class_init (AtrebasPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = atrebas_preferences_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Atrebas/ui/atrebas-preferences-window.ui");
  gtk_widget_class_bind_template_child (widget_class, AtrebasPreferencesWindow, general_page);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPreferencesWindow, background_switch);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPreferencesWindow, location_row);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPreferencesWindow, notification_switch);
}

static void
atrebas_preferences_window_init (AtrebasPreferencesWindow *self)
{
  self->settings = g_settings_new ("ca.andyholmes.Atrebas");

  gtk_widget_init_template (GTK_WIDGET (self));

  g_settings_bind (self->settings,          "background",
                   self->background_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings,        "location-services",
                   self->location_row,    "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings,            "notifications",
                   self->notification_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

