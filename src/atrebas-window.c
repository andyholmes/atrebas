// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-window"

#include "config.h"

#include <adwaita.h>
#include <geoclue.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "atrebas-backend.h"
#include "atrebas-bookmarks.h"
#include "atrebas-feature.h"
#include "atrebas-feature-layer.h"
#include "atrebas-legend.h"
#include "atrebas-macros.h"
#include "atrebas-map-view.h"
#include "atrebas-place-bar.h"
#include "atrebas-preferences-window.h"
#include "atrebas-search-model.h"
#include "atrebas-window.h"

#define VIEW_MAP       "map"
#define VIEW_LEGEND    "legend"
#define VIEW_BOOKMARKS "bookmarks"
#define VIEW_SEARCH    "search"


struct _AtrebasWindow
{
  AdwApplicationWindow  parent_instance;
  GSettings            *settings;

  /* Location Services */
  GClueSimple          *simple;
  GCancellable         *cancellable;
  GtkWindow            *preferences;

  /* Template widgets */
  AdwFlap              *flap;
  GtkStack             *flap_stack;
  AtrebasMapView           *map_view;
  GtkNoSelection       *bookmarks;
  GtkWidget            *search_entry;
  GListModel           *search_model;
};

G_DEFINE_TYPE (AtrebasWindow, atrebas_window, ADW_TYPE_APPLICATION_WINDOW)


static void
atrebas_window_load_position (AtrebasWindow *self)
{
  g_autoptr (GVariant) value = NULL;
  double latitude = 0.0;
  double longitude = 0.0;
  double zoom = 0.0;

  g_assert (ATREBAS_IS_WINDOW (self));

  value = g_settings_get_value (self->settings, "last-position");
  g_variant_get (value, "(ddd)", &latitude, &longitude, &zoom);
  g_object_set (self->map_view,
                "latitude",  latitude,
                "longitude", longitude,
                "zoom",      MAX (2.0, zoom),
                NULL);

}

static void
atrebas_window_save_position (AtrebasWindow *self)
{
  GVariant *value = NULL;
  double latitude = 0.0;
  double longitude = 0.0;
  double zoom = 0.0;

  g_assert (ATREBAS_IS_WINDOW (self));

  g_object_get (self->map_view,
                "latitude",  &latitude,
                "longitude", &longitude,
                "zoom",      &zoom,
                NULL);

  value = g_variant_new ("(ddd)", latitude, longitude, zoom);
  g_settings_set_value (self->settings, "last-position", value);
}

/*
 * Template Callbacks
 */
static void
on_place_activated (GtkListView  *list,
                    unsigned int  position,
                    AtrebasWindow    *self)
{
  g_autoptr (GeocodePlace) place = NULL;
  GListModel *model;

  g_assert (ATREBAS_IS_WINDOW (self));

  model = G_LIST_MODEL (gtk_list_view_get_model (list));
  place = g_list_model_get_item (model, position);
  atrebas_map_view_set_place (self->map_view, place);

  gtk_widget_activate_action (GTK_WIDGET (list), "win.switcher", "s", VIEW_MAP);
}

static void
on_position_changed (AtrebasMapView *view,
                     GParamSpec *pspec,
                     AtrebasWindow  *self)
{
  g_assert (ATREBAS_IS_MAP_VIEW (view));
  g_assert (ATREBAS_IS_WINDOW (self));

  atrebas_window_save_position (self);
}

static void
on_search_changed (GtkSearchEntry *entry,
                   AtrebasWindow      *self)
{
  const char *source;
  const char *target;
  const char *query;

  g_assert (GTK_IS_SEARCH_ENTRY (entry));
  g_assert (ATREBAS_IS_WINDOW (self));

  query = gtk_editable_get_text (GTK_EDITABLE (entry));
  atrebas_search_model_set_query (ATREBAS_SEARCH_MODEL (self->search_model), query);

  source = gtk_stack_get_visible_child_name (self->flap_stack);
  target = VIEW_SEARCH;

  if (atrebas_str_empty0 (query))
    target = VIEW_MAP;

  if (g_str_equal (source, target))
    return;

  gtk_widget_activate_action (GTK_WIDGET (self), "win.switcher", "s", target);
}

static void
on_search_stopped (GtkSearchEntry *entry,
                   AtrebasWindow      *self)
{
  g_assert (GTK_IS_SEARCH_ENTRY (entry));
  g_assert (ATREBAS_IS_WINDOW (self));

  gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static void
on_search_next (GtkSearchEntry *entry,
                AtrebasWindow      *self)
{
  g_assert (GTK_IS_SEARCH_ENTRY (entry));
  g_assert (ATREBAS_IS_WINDOW (self));
}

static void
on_search_previous (GtkSearchEntry *entry,
                    AtrebasWindow      *self)
{
  g_assert (GTK_IS_SEARCH_ENTRY (entry));
  g_assert (ATREBAS_IS_WINDOW (self));
}

/*
 * GeoClue2
 */
static void
on_location_changed (GClueSimple *simple,
                     GParamSpec  *pspec,
                     AtrebasWindow   *self)
{
  GClueLocation *location = NULL;

  g_assert (GCLUE_IS_SIMPLE (simple));
  g_assert (ATREBAS_IS_WINDOW (self));

  location = gclue_simple_get_location (simple);

  if (GCLUE_IS_LOCATION (location))
    {
      atrebas_map_view_set_current_location (self->map_view,
                                         gclue_location_get_latitude (location),
                                         gclue_location_get_longitude (location),
                                         gclue_location_get_accuracy (location));
    }
}

static void
gclue_simple_new_cb (GObject      *source_object,
                     GAsyncResult *result,
                     AtrebasWindow    *self)
{
  GClueSimple *simple = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (ATREBAS_IS_WINDOW (self));

  /* The operation failed */
  if ((simple = gclue_simple_new_finish (result, &error)) == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("Location Services: %s", error->message);

      return;
    }

  /* The operation succeeded, but we're awaiting a newer one */
  if (self->cancellable != g_task_get_cancellable (G_TASK (result)))
    {
      g_clear_object (&simple);
      return;
    }

  /* Location Services available */
  self->simple = g_steal_pointer (&simple);
  g_signal_connect (self->simple,
                    "notify::location",
                    G_CALLBACK (on_location_changed),
                    self);

  on_location_changed (self->simple, NULL, self);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.locate", TRUE);
}

static void
on_location_services_changed (GSettings  *settings,
                              const char *key,
                              AtrebasWindow  *self)
{
  g_assert (ATREBAS_IS_WINDOW (self));

  /* Disable the action until location services resolve */
  atrebas_map_view_set_current_location (self->map_view, 0.0, 0.0, 0.0);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.locate", FALSE);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (g_settings_get_boolean (settings, key))
    {
      self->cancellable = g_cancellable_new ();
      gclue_simple_new (APPLICATION_ID,
                        GCLUE_ACCURACY_LEVEL_EXACT,
                        self->cancellable,
                        (GAsyncReadyCallback)gclue_simple_new_cb,
                        self);
    }
  else if (self->simple != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->simple, self);
      g_clear_object (&self->simple);
    }
}


/*
 * GActions
 */
static void
switcher_action (GSimpleAction *action,
                 GVariant      *value,
                 gpointer       user_data)
{
  AtrebasWindow *self = ATREBAS_WINDOW (user_data);
  const char *target;

  g_assert (ATREBAS_IS_WINDOW (self));

  target = g_variant_get_string (value, NULL);

  if (atrebas_str_equal0 (target, VIEW_BOOKMARKS) ||
      atrebas_str_equal0 (target, VIEW_LEGEND) ||
      atrebas_str_equal0 (target, VIEW_SEARCH))
    {
      gtk_stack_set_visible_child_name (self->flap_stack, target);
      adw_flap_set_reveal_flap (self->flap, TRUE);
    }
  else
    {
      adw_flap_set_reveal_flap (self->flap, FALSE);
    }

  g_simple_action_set_state (action, value);
}

static void
locate_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *parameter)
{
  AtrebasWindow *self = ATREBAS_WINDOW (widget);
  GClueLocation *location;

  g_assert (ATREBAS_IS_WINDOW (self));
  g_return_if_fail (GCLUE_IS_SIMPLE (self->simple));

  location = gclue_simple_get_location (self->simple);
  g_object_set (self->map_view,
                "latitude",  gclue_location_get_latitude (location),
                "longitude", gclue_location_get_longitude (location),
                "zoom",      ATREBAS_MAP_VIEW_DEFAULT_ZOOM,
                NULL);

  gtk_widget_activate_action (GTK_WIDGET (self), "win.switcher", "s", VIEW_MAP);
}

static void
search_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *parameter)
{
  AtrebasWindow *self = ATREBAS_WINDOW (widget);
  const char *query;

  g_assert (ATREBAS_IS_WINDOW (self));

  query = g_variant_get_string (parameter, NULL);
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), query);
}

static void
preferences_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameter)
{
  AtrebasWindow *self = ATREBAS_WINDOW (widget);

  g_assert (ATREBAS_IS_WINDOW (self));

  if (self->preferences == NULL)
    {
      self->preferences = g_object_new (ATREBAS_TYPE_PREFERENCES_WINDOW,
                                        "application",   g_application_get_default (),
                                        "modal",         TRUE,
                                        "transient-for", self,
                                        NULL);
      g_object_add_weak_pointer (G_OBJECT (self->preferences),
                                 (gpointer)&self->preferences);
    }

  gtk_window_present (self->preferences);
}

static const GActionEntry actions[] = {
  {"switcher", NULL, "s", "'map'", switcher_action },
};


/*
 * GObject
 */
static void
atrebas_window_dispose (GObject *object)
{
  AtrebasWindow *self = ATREBAS_WINDOW (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (atrebas_window_parent_class)->dispose (object);
}

static void
atrebas_window_finalize (GObject *object)
{
  AtrebasWindow *self = ATREBAS_WINDOW (object);

  g_clear_object (&self->simple);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (atrebas_window_parent_class)->finalize (object);
}

static void
atrebas_window_class_init (AtrebasWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = atrebas_window_dispose;
  object_class->finalize = atrebas_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Atrebas/ui/atrebas-window.ui");
  gtk_widget_class_bind_template_child (widget_class, AtrebasWindow, bookmarks);
  gtk_widget_class_bind_template_child (widget_class, AtrebasWindow, flap);
  gtk_widget_class_bind_template_child (widget_class, AtrebasWindow, flap_stack);
  gtk_widget_class_bind_template_child (widget_class, AtrebasWindow, map_view);
  gtk_widget_class_bind_template_child (widget_class, AtrebasWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, AtrebasWindow, search_model);

  gtk_widget_class_bind_template_callback (widget_class, on_place_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_position_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_stopped);
  gtk_widget_class_bind_template_callback (widget_class, on_search_next);
  gtk_widget_class_bind_template_callback (widget_class, on_search_previous);

  gtk_widget_class_install_action (widget_class, "win.locate", NULL, locate_action);
  gtk_widget_class_install_action (widget_class, "win.preferences", NULL, preferences_action);
  gtk_widget_class_install_action (widget_class, "win.search", "s", search_action);
}

static void
atrebas_window_init (AtrebasWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  gtk_no_selection_set_model (self->bookmarks, atrebas_bookmarks_get_default ());

  /* Location Services */
  self->settings = g_settings_new ("ca.andyholmes.Atrebas");
  g_signal_connect (self->settings,
                    "changed::location-services",
                    G_CALLBACK (on_location_services_changed),
                    self);
  on_location_services_changed (self->settings, "location-services", self);

  /* Reset previous position */
  atrebas_window_load_position (self);
}

