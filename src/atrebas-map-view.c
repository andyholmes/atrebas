// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-map-view"

#include "config.h"

#include <gtk/gtk.h>
#include <shumate/shumate.h>

#include "atrebas-backend.h"
#include "atrebas-feature.h"
#include "atrebas-feature-layer.h"
#include "atrebas-map-marker.h"
#include "atrebas-map-view.h"
#include "atrebas-place-bar.h"
#include "atrebas-place-header.h"
#include "atrebas-utils.h"


/**
 * SECTION:atrebasmapview
 * @short_description: A map view widget
 * @title: AtrebasMapView
 * @stability: Unstable
 *
 * #AtrebasMapView is a widget for displaying one or more map boundaries,
 * overlaid on imagery from OpenStreetMap.org.
 */

struct _AtrebasMapView
{
  GtkBox              parent_instance;

  GeocodePlace       *place;
  AtrebasPlaceBar        *placebar;
  GListStore         *layers;
  GCancellable       *cancellable;
  double              latitude;
  double              longitude;
  double              zoom;

  /* Template Widgets */
  ShumateMap         *map;
  ShumateViewport    *viewport;
  ShumateMapLayer    *tiles;
  ShumateMarkerLayer *markers;
  ShumateMarker      *current_location;
  ShumateMarker      *focused_location;

  /* Widget Data */
  unsigned int        compact : 1;
  unsigned int        update_id;
  double              pointer_x;
  double              pointer_y;
};

static void   atrebas_map_view_resolve (AtrebasMapView *self);
static void   atrebas_map_view_update  (AtrebasMapView *self);

G_DEFINE_TYPE (AtrebasMapView, atrebas_map_view, GTK_TYPE_BOX);


enum {
  PROP_0,
  PROP_COMPACT,
  PROP_LATITUDE,
  PROP_LAYERS,
  PROP_LONGITUDE,
  PROP_ZOOM,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * Template Callbacks
 */
static void
on_place_activated (GtkListView  *list,
                    unsigned int  position,
                    AtrebasMapView   *self)
{
  g_autoptr (GeocodePlace) place = NULL;
  GtkSelectionModel *selection = NULL;

  g_assert (GTK_IS_LIST_VIEW (list));
  g_assert (ATREBAS_IS_MAP_VIEW (self));

  selection = gtk_list_view_get_model (list);
  place = g_list_model_get_item (G_LIST_MODEL (selection), position);

  atrebas_map_view_set_place (self, place);
}

static void
on_pointer_released (GtkGestureClick *gesture,
                     int              n_press,
                     double           x,
                     double           y,
                     AtrebasMapView      *self)
{
  g_assert (ATREBAS_IS_MAP_VIEW (self));

  /* Pick the point at the time of the second click */
  if (n_press == 2)
    {
      self->pointer_x = x;
      self->pointer_y = y;
    }

  if (n_press >= 2 && self->pointer_x != 0.0 && self->pointer_y != 0.0)
    {
      g_clear_object (&self->place);

      shumate_viewport_widget_coords_to_location (self->viewport,
                                                  GTK_WIDGET (self->map),
                                                  self->pointer_x,
                                                  self->pointer_y,
                                                  &self->latitude,
                                                  &self->longitude);
      atrebas_map_view_update (self);

      self->pointer_x = 0.0;
      self->pointer_y = 0.0;
    }
}

static void
on_pointer_stopped (GtkGestureClick *gesture,
                    AtrebasMapView      *self)
{
  g_assert (ATREBAS_IS_MAP_VIEW (self));

  self->pointer_x = 0.0;
  self->pointer_y = 0.0;
}

static gboolean
on_marker_activated (AtrebasMapMarker *marker,
                     AtrebasMapView   *self)
{
  g_autoptr (GeocodePlace) place = NULL;
  double latitude = 0.0;
  double longitude = 0.0;

  g_assert (ATREBAS_IS_MAP_MARKER (marker));
  g_assert (ATREBAS_IS_MAP_VIEW (self));

  g_object_get (marker,
                "place",     &place,
                "latitude",  &latitude,
                "longitude", &longitude,
                NULL);

  if (GEOCODE_IS_PLACE (place))
    {
      atrebas_place_bar_set_place (self->placebar, place);
    }
  else if (latitude != 0.0 || longitude != 0.0)
    {
      g_object_set (self->placebar,
                    "latitude",  latitude,
                    "longitude", longitude,
                    NULL);
    }

  /* If the UI is in compact mode, we're returning %TRUE to stop the popover */
  return self->compact;
}


/*
 * AtrebasMapView
 */
static int
feature_list_sort (AtrebasFeatureLayer *layer1,
                   AtrebasFeatureLayer *layer2,
                   AtrebasMapView      *self)
{
  AtrebasFeature *feature1 = atrebas_feature_layer_get_feature (layer1);
  AtrebasFeature *feature2 = atrebas_feature_layer_get_feature (layer2);
  int theme = 0;

  theme = atrebas_feature_get_theme (feature1) - atrebas_feature_get_theme (feature2);

  if (theme != 0)
    return theme;

  return g_utf8_collate (geocode_place_get_name (GEOCODE_PLACE (feature1)),
                         geocode_place_get_name (GEOCODE_PLACE (feature2)));
}

static void
atrebas_map_view_set_focus (AtrebasMapView *self,
                        double      latitude,
                        double      longitude)
{
  g_assert (ATREBAS_IS_MAP_VIEW (self));
  g_assert (ATREBAS_IS_LATITUDE (latitude));
  g_assert (ATREBAS_IS_LONGITUDE (longitude));

  if (GEOCODE_IS_PLACE (self->place) && !ATREBAS_IS_FEATURE (self->place))
    {
      g_object_set (self->focused_location,
                    "place",   self->place,
                    "visible", TRUE,
                    NULL);

      g_object_set (self->placebar,
                    "place", self->place,
                    NULL);
    }
  else if (latitude != 0.0 || longitude != 0.0)
    {
      g_object_set (self->focused_location,
                    "latitude",  latitude,
                    "longitude", longitude,
                    "visible", TRUE,
                    NULL);

      g_object_set (self->placebar,
                    "latitude",  latitude,
                    "longitude", longitude,
                    NULL);
    }
  else
    {
      g_object_set (self->focused_location,
                    "visible", FALSE,
                    NULL);
    }
}

static double
atrebas_map_view_get_zoom_for_place (AtrebasMapView   *self,
                                 GeocodePlace *place)
{
  ShumateMapSource *source;
  gboolean good = FALSE;
  double left, top, right, bottom;
  double height, width, min_zoom, max_zoom, zoom;

  height = gtk_widget_get_height (GTK_WIDGET (self));
  width = gtk_widget_get_width (GTK_WIDGET (self));
  source = shumate_viewport_get_reference_map_source (self->viewport);
  g_object_get (self->viewport,
                "max-zoom-level", &max_zoom,
                "min-zoom-level", &min_zoom,
                "zoom-level",     &zoom,
                NULL);

  g_object_get (geocode_place_get_bounding_box (place),
                "left",   &left,
                "bottom", &bottom,
                "right",  &right,
                "top",    &top,
                NULL);

  zoom = ATREBAS_MAP_VIEW_MAX_ZOOM;

  while (!good)
    {
      double min_x = shumate_map_source_get_x (source, zoom, left);
      double min_y = shumate_map_source_get_y (source, zoom, bottom);
      double max_x = shumate_map_source_get_x (source, zoom, right);
      double max_y = shumate_map_source_get_y (source, zoom, top);

      if (min_y - max_y <= height && max_x - min_x <= width)
        good = TRUE;
      else
        zoom -= 1;

      if (zoom <= min_zoom)
        return min_zoom;
    }

  return zoom;
}

static void
reverse_resolve_cb (GeocodeBackend *backend,
                    GAsyncResult   *result,
                    AtrebasMapView     *self)
{
  g_autolist (GeocodePlace) ret = NULL;
  g_autoptr (GError) error = NULL;

  if (self->cancellable != g_task_get_cancellable (G_TASK (result)))
    return;

  ret = geocode_backend_reverse_resolve_finish (backend, result, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("%s: %s", G_STRFUNC, error->message);

      return;
    }

  /* HACK: ensure the target feature ends up in the list */
  if (ATREBAS_IS_FEATURE (self->place))
    {
      gboolean found = FALSE;

      for (const GList *iter = ret; iter; iter = iter->next)
        {
          GeocodePlace *place = iter->data;

          if (atrebas_feature_equal (self->place, place))
            {
              g_set_object (&self->place, place);
              found = TRUE;
              break;
            }
        }

      if (!found)
        ret = g_list_prepend (ret, g_object_ref (self->place));
    }

  /* Add new features */
  for (const GList *iter = ret; iter; iter = iter->next)
    {
      AtrebasFeature *feature = ATREBAS_FEATURE (iter->data);
      GtkWidget *layer;

      /* Add a feature layer, ensuring the marker stays on top */
      layer = atrebas_feature_layer_new (self->viewport, feature);
      shumate_map_insert_layer_behind (self->map,
                                       SHUMATE_LAYER (layer),
                                       SHUMATE_LAYER (self->markers));

      /* When we target a feature, ensure only its layer is initially visible */
      if (ATREBAS_IS_FEATURE (self->place))
        gtk_widget_set_sensitive (GTK_WIDGET (layer),
                                  atrebas_feature_equal (self->place, feature));
      else
        gtk_widget_set_sensitive (GTK_WIDGET (layer), TRUE);

      g_list_store_insert_sorted (self->layers,
                                  layer,
                                  (GCompareDataFunc)feature_list_sort,
                                  self);
    }

  /* Don't show a marker when the focus is a feature */
  if (!ATREBAS_IS_FEATURE (self->place))
    atrebas_map_view_set_focus (self, self->latitude, self->longitude);
}

static void
atrebas_map_view_resolve (AtrebasMapView *self)
{
  g_autoptr (GHashTable) params = NULL;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  /* Query features */
  params = atrebas_geocode_parameters_for_coordinates (self->latitude,
                                                   self->longitude);
  geocode_backend_reverse_resolve_async (atrebas_backend_get_default (),
                                         params,
                                         self->cancellable,
                                         (GAsyncReadyCallback)reverse_resolve_cb,
                                         self);

  g_object_set (self->placebar,
                "latitude",  self->latitude,
                "longitude", self->longitude,
                NULL);
}

static gboolean
atrebas_map_view_update_idle (gpointer data)
{
  AtrebasMapView *self = ATREBAS_MAP_VIEW (data);

  g_assert (ATREBAS_IS_MAP_VIEW (self));

  if (self->latitude == 0.0 || self->longitude == 0.0)
    self->zoom = 0.0;

  g_clear_object (&self->place);
  atrebas_map_view_clear (self);

  /* TODO: animations */
  /* shumate_map_go_to (self->map, self->latitude, self->longitude); */
  g_object_set (self->viewport,
                "latitude",   self->latitude,
                "longitude",  self->longitude,
                "zoom-level", self->zoom,
                NULL);

  atrebas_map_view_resolve (self);

  self->update_id = 0;

  return G_SOURCE_REMOVE;
}

static void
atrebas_map_view_update (AtrebasMapView *view)
{
  g_return_if_fail (ATREBAS_IS_MAP_VIEW (view));

  if (view->update_id == 0)
    {
      view->update_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                         atrebas_map_view_update_idle,
                                         g_object_ref (view),
                                         g_object_unref);
    }
}


/*
 * GObject
 */
static void
atrebas_map_view_dispose (GObject *object)
{
  AtrebasMapView *self = ATREBAS_MAP_VIEW (object);

  g_clear_handle_id (&self->update_id, g_source_remove);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  G_OBJECT_CLASS (atrebas_map_view_parent_class)->dispose (object);
}

static void
atrebas_map_view_finalize (GObject *object)
{
  AtrebasMapView *self = ATREBAS_MAP_VIEW (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->layers);
  g_clear_object (&self->place);

  G_OBJECT_CLASS (atrebas_map_view_parent_class)->finalize (object);
}

static void
atrebas_map_view_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  AtrebasMapView *self = ATREBAS_MAP_VIEW (object);

  switch (prop_id)
    {
    case PROP_COMPACT:
      g_value_set_boolean (value, self->compact);
      break;

    case PROP_LATITUDE:
      g_value_set_double (value, self->latitude);
      break;

    case PROP_LAYERS:
      g_value_set_object (value, self->layers);
      break;

    case PROP_LONGITUDE:
      g_value_set_double (value, self->longitude);
      break;

    case PROP_ZOOM:
      g_value_set_double (value, self->zoom);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_map_view_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  AtrebasMapView *self = ATREBAS_MAP_VIEW (object);

  switch (prop_id)
    {
    case PROP_COMPACT:
      atrebas_map_view_set_compact (self, g_value_get_boolean (value));
      break;

    case PROP_LATITUDE:
      atrebas_map_view_set_latitude (self, g_value_get_double (value));
      break;

    case PROP_LONGITUDE:
      atrebas_map_view_set_longitude (self, g_value_get_double (value));
      break;

    case PROP_ZOOM:
      atrebas_map_view_set_zoom (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_map_view_class_init (AtrebasMapViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = atrebas_map_view_dispose;
  object_class->finalize = atrebas_map_view_finalize;
  object_class->get_property = atrebas_map_view_get_property;
  object_class->set_property = atrebas_map_view_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Atrebas/ui/atrebas-map-view.ui");
  gtk_widget_class_bind_template_child (widget_class, AtrebasMapView, map);
  gtk_widget_class_bind_template_child (widget_class, AtrebasMapView, placebar);
  gtk_widget_class_bind_template_callback (widget_class, on_place_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_pointer_released);
  gtk_widget_class_bind_template_callback (widget_class, on_pointer_stopped);

  /**
   * AtrebasMapView:compact:
   *
   * Whether the widget is in compact mode.
   */
  properties [PROP_COMPACT] =
    g_param_spec_boolean ("compact",
                          "Compact",
                          "Whether the widget is in compact mode.",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasMapView:latitude:
   *
   * The north–south position of a point.
   */
  properties [PROP_LATITUDE] =
    g_param_spec_double ("latitude",
                         "Latitude",
                         "The north–south position of a point.",
                         SHUMATE_MIN_LATITUDE, SHUMATE_MAX_LATITUDE,
                         0.0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasMapView:layers:
   *
   * The #ShumateLayers shown on the map.
   */
  properties [PROP_LAYERS] =
    g_param_spec_object ("layers",
                         "Layers",
                         "The layers shown on the map.",
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasMapView:longitude:
   *
   * The east-west position of a point.
   */
  properties [PROP_LONGITUDE] =
    g_param_spec_double ("longitude",
                         "Longitude",
                         "The east-west position of a point.",
                         SHUMATE_MIN_LONGITUDE, SHUMATE_MAX_LONGITUDE,
                         0.0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasMapView:zoom:
   *
   * The zoom level of the map.
   */
  properties [PROP_ZOOM] =
    g_param_spec_double ("zoom",
                         "Zoom",
                         "The zoom level of the map.",
                         ATREBAS_MAP_VIEW_MIN_ZOOM, ATREBAS_MAP_VIEW_MAX_ZOOM,
                         ATREBAS_MAP_VIEW_DEFAULT_ZOOM,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_map_view_init (AtrebasMapView *self)
{
  g_autoptr (ShumateMapSourceRegistry) registry = NULL;
  ShumateMapSource *source;

  self->layers = g_list_store_new (SHUMATE_TYPE_LAYER);
  self->latitude = 0.0;
  self->longitude = 0.0;
  self->zoom = ATREBAS_MAP_VIEW_DEFAULT_ZOOM;

  gtk_widget_init_template (GTK_WIDGET (self));

// FIXME: GLib-GIO-ERROR **: Settings schema 'org.gnome.system.proxy' is not installed
  /* Set the map source */
  registry = shumate_map_source_registry_new_with_defaults ();
  source = shumate_map_source_registry_get_by_id (registry,
                                                  SHUMATE_MAP_SOURCE_OSM_MAPNIK);
  shumate_map_set_map_source (self->map, source);

  self->viewport = shumate_map_get_viewport (self->map);
  shumate_viewport_set_min_zoom_level (self->viewport, 2.0);

  /* Default Layers */
  self->tiles = shumate_map_layer_new (source, self->viewport);
  shumate_map_add_layer (self->map, SHUMATE_LAYER (self->tiles));

  /* Add markers for the actual and focused location */
  self->markers = shumate_marker_layer_new (self->viewport);
  shumate_map_add_layer (self->map, SHUMATE_LAYER (self->markers));

  /* Current Location (ie. GeoClue2 or other location services) */
  self->current_location = g_object_new (ATREBAS_TYPE_MAP_MARKER,
                                         "icon-name", "find-location-symbolic",
                                         "visible",   FALSE,
                                         NULL);
  shumate_marker_layer_add_marker (self->markers, self->current_location);

  /* Focused Location (ie. "What's Here?") */
  self->focused_location = g_object_new (ATREBAS_TYPE_MAP_MARKER,
                                        "icon-name", "atrebas-location-symbolic",
                                        "visible",   FALSE,
                                        NULL);
  shumate_marker_layer_add_marker (self->markers, self->focused_location);

  /* When a marker is activated, we may want to interrupt the popover and
   * use the place bar if the UI is in compact mode */
  g_signal_connect (self->current_location,
                    "activate",
                    G_CALLBACK (on_marker_activated),
                    self);
  g_signal_connect (self->focused_location,
                    "activate",
                    G_CALLBACK (on_marker_activated),
                    self);
}

/**
 * atrebas_map_view_new:
 *
 * Create a new #AtrebasMapView.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
atrebas_map_view_new (void)
{
  return g_object_new (ATREBAS_TYPE_MAP_VIEW, NULL);
}

/**
 * atrebas_map_view_get_compact:
 * @view: a #AtrebasMapView
 *
 * Get whether @view is in compact mode.
 *
 * Returns: %TRUE if the UI is compact, %FALSE otherwise
 */
gboolean
atrebas_map_view_get_compact (AtrebasMapView *view)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_VIEW (view), FALSE);

  return view->compact;
}

/**
 * atrebas_map_view_set_compact:
 * @view: a #AtrebasMapView
 * @compact: set the display mode
 *
 * Set whether @view is in compact mode.
 */
void
atrebas_map_view_set_compact (AtrebasMapView *view,
                          gboolean    compact)
{
  g_return_if_fail (ATREBAS_IS_MAP_VIEW (view));

  if (view->compact == compact)
    return;

  view->compact = compact;
  g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_COMPACT]);
}

/**
 * atrebas_map_view_get_latitude:
 * @view: a #AtrebasMapView
 *
 * Get the north-south position of @view.
 *
 * Returns: a latitude
 */
double
atrebas_map_view_get_latitude (AtrebasMapView *view)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_VIEW (view), 0.0);

  return view->latitude;
}

/**
 * atrebas_map_view_set_latitude:
 * @view_view: a #AtrebasMapView
 * @latitude: a latitude
 *
 * Set the north-south position of @view to @latitude.
 */
void
atrebas_map_view_set_latitude (AtrebasMapView *view,
                           double      latitude)
{
  g_return_if_fail (ATREBAS_IS_MAP_VIEW (view));
  g_return_if_fail (latitude >= -90.0 && latitude <= 90.0);

  if (view->latitude == latitude)
    return;

  view->latitude = latitude;
  g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_LATITUDE]);

  atrebas_map_view_update (view);
}

/**
 * atrebas_map_view_get_longitude:
 * @view: a #AtrebasMapView
 *
 * Get the east-west position of @view.
 *
 * Returns: a longitude
 */
double
atrebas_map_view_get_longitude (AtrebasMapView *view)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_VIEW (view), 0.0);

  return view->longitude;
}

/**
 * atrebas_map_view_set_longitude:
 * @view: a #AtrebasMapView
 * @longitude: a longitude
 *
 * Set the east-west of @view to @longitude.
 */
void
atrebas_map_view_set_longitude (AtrebasMapView *view,
                            double      longitude)
{
  g_return_if_fail (ATREBAS_IS_MAP_VIEW (view));
  g_return_if_fail (longitude >= -180.0 && longitude <= 180.0);

  if (view->longitude == longitude)
    return;

  view->longitude = longitude;
  g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_LONGITUDE]);

  atrebas_map_view_update (view);
}

/**
 * atrebas_map_view_get_layers:
 * @view: a #AtrebasMapView
 *
 * Get a #GListModel of feature layers.
 *
 * Returns: (transfer none): a #GListModel
 */
GListModel *
atrebas_map_view_get_layers (AtrebasMapView *view)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_VIEW (view), FALSE);

  return G_LIST_MODEL (view->layers);
}

/**
 * atrebas_map_view_get_zoom:
 * @view: a #AtrebasMapView
 *
 * Get the zoom level of @view.
 *
 * Returns: a zoom level
 */
double
atrebas_map_view_get_zoom (AtrebasMapView *view)
{
  g_return_val_if_fail (ATREBAS_IS_MAP_VIEW (view), 0.0);

  return view->zoom;
}

/**
 * atrebas_map_view_set_zoom:
 * @view: a #AtrebasMapView
 * @zoom: a zoom level
 *
 * Get the zoom level of @view to @zoom.
 */
void
atrebas_map_view_set_zoom (AtrebasMapView *view,
                       double      zoom)
{
  g_return_if_fail (ATREBAS_IS_MAP_VIEW (view));
  g_return_if_fail (zoom >= 0.0 && zoom <= 20.0);

  if (view->zoom == zoom)
    return;

  view->zoom = zoom;
  g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_ZOOM]);

  atrebas_map_view_update (view);
}

/**
 * atrebas_map_view_set_place:
 * @view: a #AtrebasMapView
 * @feature: (nullable): a #GeocodePlace
 *
 * Show the #ShumatePathLayer representing the geometry of @feature.
 */
void
atrebas_map_view_set_place (AtrebasMapView   *view,
                        GeocodePlace *place)
{
  GeocodeLocation *location;

  g_return_if_fail (ATREBAS_IS_MAP_VIEW (view));
  g_return_if_fail (place == NULL || GEOCODE_IS_PLACE (place));

  if (!g_set_object (&view->place, place))
    return;

  atrebas_map_view_clear (view);

  /* TODO: Attempt to find the optimal position and zoom */
  location = geocode_place_get_location (place);
  g_object_get (location,
                "latitude",  &view->latitude,
                "longitude", &view->longitude,
                NULL);
  view->zoom = atrebas_map_view_get_zoom_for_place (view, place);

  /* Set the position */
  g_object_set (view->viewport,
                "latitude",   view->latitude,
                "longitude",  view->longitude,
                "zoom-level", view->zoom,
                NULL);
  atrebas_map_view_resolve (view);
  atrebas_place_bar_set_place (view->placebar, view->place);
}

/**
 * atrebas_map_view_set_current_location:
 * @view: a #AtrebasMapView
 * @latitude: a north-south position
 * @longitude: an east-west position
 * @accuracy: a range in meters
 *
 * Set the current location, as returned by location services (ie. GeoClue2). A
 * small marker will be placed on the map, indicating the current position.
 *
 * If both @latitude and @longitude are `0.0`, the marker will be hidden.
 */
void
atrebas_map_view_set_current_location (AtrebasMapView *view,
                                   double      latitude,
                                   double      longitude,
                                   double      accuracy)
{
  g_return_if_fail (ATREBAS_IS_MAP_VIEW (view));
  g_return_if_fail (latitude >= ATREBAS_MIN_LATITUDE && latitude <= ATREBAS_MAX_LATITUDE);
  g_return_if_fail (longitude >= ATREBAS_MIN_LONGITUDE && longitude <= ATREBAS_MAX_LONGITUDE);

  g_object_set (view->current_location,
                "place",     NULL,
                "latitude",  latitude,
                "longitude", longitude,
                "visible",   (latitude != 0.0 || longitude != 0.0),
                NULL);
}

/**
 * atrebas_map_view_set_focused_location:
 * @view: a #AtrebasMapView
 * @latitude: a north-south position
 * @longitude: an east-west position
 * @accuracy: a range in meters
 *
 * Set the focused location, which will be displayed in the place bar and other
 * UI elements. A small marker will be placed on the map, indicating the focused
 * position.
 *
 * If both @latitude and @longitude are `0.0`, the marker will be hidden.
 */
void
atrebas_map_view_set_focused_location (AtrebasMapView *view,
                                   double      latitude,
                                   double      longitude,
                                   double      accuracy)
{
  g_return_if_fail (ATREBAS_IS_MAP_VIEW (view));
  g_return_if_fail (latitude >= ATREBAS_MIN_LATITUDE && latitude <= ATREBAS_MAX_LATITUDE);
  g_return_if_fail (longitude >= ATREBAS_MIN_LONGITUDE && longitude <= ATREBAS_MAX_LONGITUDE);

  g_object_set (view->focused_location,
                "place",     NULL,
                "latitude",  latitude,
                "longitude", longitude,
                "visible",   (latitude != 0.0 || longitude != 0.0),
                NULL);
}

/**
 * atrebas_map_view_clear:
 * @view: a #AtrebasMapView
 *
 * Clear all features and markers from the map.
 */
void
atrebas_map_view_clear (AtrebasMapView *self)
{
  unsigned int n_layers;

  g_assert (ATREBAS_IS_MAP_VIEW (self));

  atrebas_map_view_set_focus (self, 0.0, 0.0);

  /* Remove map layers */
  n_layers = g_list_model_get_n_items (G_LIST_MODEL (self->layers));

  for (unsigned int i = 0; i < n_layers; i++)
    {
      g_autoptr (ShumateLayer) layer = NULL;

      layer = g_list_model_get_item (G_LIST_MODEL (self->layers), i);
      shumate_map_remove_layer (self->map, layer);
    }
  g_list_store_remove_all (self->layers);
}

