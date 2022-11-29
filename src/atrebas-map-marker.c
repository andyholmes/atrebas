// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-map-marker"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <shumate/shumate.h>

#include "atrebas-bookmarks.h"
#include "atrebas-feature.h"
#include "atrebas-map-marker.h"
#include "atrebas-search-model.h"


/**
 * SECTION:atrebasmapmarker
 * @short_description: A location marker
 * @title: AtrebasMapMarker
 * @stability: Unstable
 *
 * #AtrebasMapMarker is used as a visual indicator of a position on the map.
 */

struct _AtrebasMapMarker
{
  ShumateMarker  parent_instance;

  GeocodePlace  *place;
  GListModel    *features;
  char          *icon_name;
  int            icon_size;

  /* Template Widgets */
  GtkWidget     *popover;
  GtkStack      *stack;
  GtkLabel      *error_label;
};

G_DEFINE_TYPE (AtrebasMapMarker, atrebas_map_marker, SHUMATE_TYPE_MARKER)

enum {
  PROP_0,
  PROP_FEATURES,
  PROP_PLACE,
  PROP_ICON_NAME,
  PROP_ICON_SIZE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  ACTIVATE,
  N_SIGNALS,
};

static unsigned int signals[N_SIGNALS] = { 0, };


static void
atrebas_map_marker_refresh (AtrebasMapMarker *marker)
{
  gtk_widget_action_set_enabled (GTK_WIDGET (marker),
                                 "place.info",
                                 ATREBAS_IS_FEATURE (marker->place));

  if (marker->place)
    gtk_stack_set_visible_child_name (marker->stack, "info");
}

static void
reverse_resolve_cb (GeocodeReverse *reverse,
                    GAsyncResult   *result,
                    AtrebasMapMarker   *self)
{
  g_autoptr (GeocodePlace) place = NULL;
  g_autoptr (GError) error = NULL;

  place = geocode_reverse_resolve_finish (reverse, result, &error);

  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      gtk_label_set_label (self->error_label, error->message);
      gtk_stack_set_visible_child_name (self->stack, "error");
    }

  /* We don't want to trample the current latitude & longitude by calling
   * atrebas_map_marker_set_place(), so we do this manually */
  if (g_set_object (&self->place, place))
    {
      atrebas_map_marker_refresh (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PLACE]);
    }
}

static void
atrebas_map_marker_resolve (AtrebasMapMarker *self)
{
  double latitude;
  double longitude;

  g_assert (ATREBAS_IS_MAP_MARKER (self));

  latitude = shumate_location_get_latitude (SHUMATE_LOCATION (self));
  longitude = shumate_location_get_longitude (SHUMATE_LOCATION (self));

  /* Ensure the #GeocodePlace is not stale */
  if (GEOCODE_IS_PLACE (self->place))
    {
      GeocodeLocation *location = geocode_place_get_location (self->place);
      double place_latitude = 0.0;
      double place_longitude = 0.0;

      if (GEOCODE_IS_LOCATION (location))
        {
          g_object_get (location,
                        "latitude",  &place_latitude,
                        "longitude", &place_longitude,
                        NULL);
        }

      if (latitude != place_latitude || longitude != place_longitude)
        g_clear_object (&self->place);
    }

  if (self->place == NULL)
    {
      g_autoptr (GeocodeLocation) location = NULL;
      g_autoptr (GeocodeReverse) reverse = NULL;

      /* Create a query for the present location of the marker */
      location = geocode_location_new (latitude, longitude, 100.0);
      reverse = geocode_reverse_new_for_location (location);

      /* Start the search and switch to the loading page */
      geocode_reverse_resolve_async (reverse,
                                     NULL,
                                     (GAsyncReadyCallback)reverse_resolve_cb,
                                     self);
      gtk_stack_set_visible_child_name (self->stack, "load");
    }

  atrebas_map_marker_refresh (self);
}

static void
on_items_changed (GListModel   *model,
                  unsigned int  position,
                  unsigned int  removed,
                  unsigned int  added,
                  AtrebasMapMarker *self)
{
  atrebas_map_marker_resolve (self);
}

static void
on_pointer_released (GtkGestureClick *gesture,
                     int              n_press,
                     double           x,
                     double           y,
                     AtrebasMapMarker    *self)
{
  gboolean ret = FALSE;

  g_assert (ATREBAS_IS_MAP_MARKER (self));

  g_signal_emit (G_OBJECT (self), signals [ACTIVATE], 0, &ret);
}


/*
 * GActions
 */
static void
reload_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *parameter)
{
  AtrebasMapMarker *self = ATREBAS_MAP_MARKER (widget);

  g_assert (ATREBAS_IS_MAP_MARKER (self));

  atrebas_map_marker_resolve (self);
}


/*
 * AtrebasMapMarker
 */
static gboolean
atrebas_map_marker_activate (AtrebasMapMarker *self)
{
  g_assert (ATREBAS_IS_MAP_MARKER (self));

  atrebas_map_marker_resolve (self);
  gtk_widget_set_visible (GTK_WIDGET (self->popover), TRUE);
  gtk_popover_popup (GTK_POPOVER (self->popover));

  return TRUE;
}


/*
 * GObject
 */
static void
atrebas_map_marker_dispose (GObject *object)
{
  AtrebasMapMarker *self = ATREBAS_MAP_MARKER (object);

  if (self->features)
    g_signal_handlers_disconnect_by_data (self->features, self);

  g_clear_pointer (&self->popover, gtk_widget_unparent);

  G_OBJECT_CLASS (atrebas_map_marker_parent_class)->dispose (object);
}

static void
atrebas_map_marker_finalize (GObject *object)
{
  AtrebasMapMarker *self = ATREBAS_MAP_MARKER (object);

  g_clear_object (&self->features);
  g_clear_object (&self->place);
  g_clear_pointer (&self->icon_name, g_free);

  G_OBJECT_CLASS (atrebas_map_marker_parent_class)->finalize (object);
}

static void
atrebas_map_marker_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  AtrebasMapMarker *self = ATREBAS_MAP_MARKER (object);

  switch (prop_id)
    {
    case PROP_FEATURES:
      g_value_set_object (value, self->features);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, self->icon_name);
      break;

    case PROP_ICON_SIZE:
      g_value_set_int (value, self->icon_size);
      break;

    case PROP_PLACE:
      g_value_set_object (value, self->place);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_map_marker_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  AtrebasMapMarker *self = ATREBAS_MAP_MARKER (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      atrebas_map_marker_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ICON_SIZE:
      atrebas_map_marker_set_icon_size (self, g_value_get_int (value));
      break;

    case PROP_PLACE:
      atrebas_map_marker_set_place (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_map_marker_class_init (AtrebasMapMarkerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = atrebas_map_marker_dispose;
  object_class->finalize = atrebas_map_marker_finalize;
  object_class->get_property = atrebas_map_marker_get_property;
  object_class->set_property = atrebas_map_marker_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Atrebas/ui/atrebas-map-marker.ui");
  gtk_widget_class_bind_template_child (widget_class, AtrebasMapMarker, popover);
  gtk_widget_class_bind_template_child (widget_class, AtrebasMapMarker, stack);
  gtk_widget_class_bind_template_callback (widget_class, on_pointer_released);
  gtk_widget_class_install_action (widget_class, "marker.reload", NULL, reload_action);

  /**
   * AtrebasMapMarker:features
   *
   * The #GeocodePlace the marker represents.
   */
  properties [PROP_FEATURES] =
    g_param_spec_object ("features",
                         "Features",
                         "The features that intersect the marker position.",
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasMapMarker:place
   *
   * The #GeocodePlace the marker represents.
   */
  properties [PROP_PLACE] =
    g_param_spec_object ("place",
                         "Place",
                         "The place the marker is at.",
                         GEOCODE_TYPE_PLACE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasMapMarker:icon-name
   *
   * The size of the marker icon.
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The icon name of the marker icon.",
                         "atrebas-location-symbolic",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasMapMarker:icon-size
   *
   * The size of the marker icon.
   */
  properties [PROP_ICON_SIZE] =
    g_param_spec_int ("icon-size",
                      "Icon Size",
                      "The size of the marker icon.",
                      -1, G_MAXINT,
                      16,
                      (G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * AtrebasMapMarker::activate:
   * @marker: a #AtrebasMapMarker
   *
   * The #AtrebasMapMarker::activate signal is emitted when @marker is activated by
   * pointer, keyboard or touch.
   *
   * Signal handlers should return %TRUE to indicate the event is handled.
   */
  signals [ACTIVATE] =
    g_signal_new_class_handler ("activate",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (atrebas_map_marker_activate),
                                g_signal_accumulator_true_handled, NULL,
                                NULL,
                                G_TYPE_BOOLEAN, 0);
}

static void
atrebas_map_marker_init (AtrebasMapMarker *self)
{
  self->features = atrebas_search_model_new (NULL);

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_set_parent (GTK_WIDGET (self->popover), GTK_WIDGET (self));

  g_signal_connect (self->features,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    self);
  g_object_bind_property (self,           "latitude",
                          self->features, "latitude",
                          G_BINDING_DEFAULT);
  g_object_bind_property (self,           "longitude",
                          self->features, "longitude",
                          G_BINDING_DEFAULT);
}

/**
 * atrebas_map_marker_new:
 * @place: (nullable): a #GeocodePlace
 *
 * Create a new #AtrebasMapMarker.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
atrebas_map_marker_new (GeocodePlace *place)
{
  return g_object_new (ATREBAS_TYPE_MAP_MARKER,
                       "place", place,
                       NULL);
}

/**
 * atrebas_map_marker_get_features:
 * @marker: a #AtrebasMapMarker
 *
 * Get the #GListModel of #GeocodePlace features that intersect the position
 * of @marker.
 *
 * Returns: (transfer none) (nullable): a #GeocodePlace
 */
GListModel *
atrebas_map_marker_get_features (AtrebasMapMarker *marker)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_MARKER (marker), NULL);

  return marker->features;
}

/**
 * atrebas_map_marker_get_place:
 * @marker: a #AtrebasMapMarker
 *
 * Get the #GeocodePlace represented by @marker.
 *
 * Returns: (transfer none) (nullable): a #GeocodePlace
 */
GeocodePlace *
atrebas_map_marker_get_place (AtrebasMapMarker *marker)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_MARKER (marker), NULL);

  return marker->place;
}

/**
 * atrebas_map_marker_set_place:
 * @marker: a #AtrebasMapMarker
 * @place: (nullable): a #GeocodePlace
 *
 * Set the #GeocodePlace represented by @marker.
 */
void
atrebas_map_marker_set_place (AtrebasMapMarker *marker,
                          GeocodePlace *place)
{
  g_return_if_fail (ATREBAS_IS_MAP_MARKER (marker));

  if (!g_set_object (&marker->place, place))
    return;

  if (GEOCODE_IS_PLACE (marker->place))
    {
      GeocodeLocation *location = geocode_place_get_location (marker->place);
      double latitude = 0.0;
      double longitude = 0.0;

      if (GEOCODE_IS_LOCATION (location))
        {
          g_object_get (geocode_place_get_location (marker->place),
                        "latitude",  &latitude,
                        "longitude", &longitude,
                        NULL);
        }

      g_object_set (SHUMATE_LOCATION (marker),
                    "latitude",  latitude,
                    "longitude", longitude,
                    NULL);
    }

  atrebas_map_marker_refresh (marker);
  g_object_notify_by_pspec (G_OBJECT (marker), properties [PROP_PLACE]);
}

/**
 * atrebas_map_marker_get_icon_name:
 * @marker: a #AtrebasMapMarker
 *
 * Get the icon name for the marker icon.
 *
 * Returns: (transfer none) (nullable): a themed icon name
 */
const char *
atrebas_map_marker_get_icon_name (AtrebasMapMarker *marker)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_MARKER (marker), NULL);

  return marker->icon_name;
}

/**
 * atrebas_map_marker_set_icon_name:
 * @marker: a #AtrebasMapMarker
 * @name: (nullable): a themed icon name
 *
 * Set the icon name for the marker icon to @name.
 */
void
atrebas_map_marker_set_icon_name (AtrebasMapMarker *marker,
                              const char   *name)
{
  g_return_if_fail (ATREBAS_IS_MAP_MARKER (marker));

  if (g_strcmp0 (marker->icon_name, name) == 0)
    return;

  g_clear_pointer (&marker->icon_name, g_free);
  marker->icon_name = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (marker), properties [PROP_ICON_NAME]);
}

/**
 * atrebas_map_marker_get_icon_size:
 * @marker: a #AtrebasMapMarker
 *
 * Get the icon size for the marker icon.
 *
 * Returns: an icon size or `-1` if unset
 */
int
atrebas_map_marker_get_icon_size (AtrebasMapMarker *marker)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_MARKER (marker), -1);

  return marker->icon_size;
}

/**
 * atrebas_map_marker_set_icon_size:
 * @marker: a #AtrebasMapMarker
 * @size: an icon size
 *
 * Set the icon size for the marker icon to @size.
 */
void
atrebas_map_marker_set_icon_size (AtrebasMapMarker *marker,
                              int           size)
{
  g_return_if_fail (ATREBAS_IS_MAP_MARKER (marker));

  if (marker->icon_size == size)
    return;

  marker->icon_size = size;
  g_object_notify_by_pspec (G_OBJECT (marker), properties [PROP_ICON_SIZE]);
}

