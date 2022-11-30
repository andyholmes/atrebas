// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <cairo/cairo-gobject.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <shumate/shumate.h>

#include "atrebas-feature.h"
#include "atrebas-feature-layer.h"

#define BORDER_ON      0
#define BORDER_OFF     1
#define BORDER_NUM     2


/**
 * AtrebasFeatureLayer:
 *
 * [class@Atrebas.FeatureLayer] is a subclass of #ShumateLayer for displaying
 * [class@Atrebas.Feature] objects as bordered overlays.
 *
 * #AtrebasFeatureLayer is loosely based on [class@Shumate.PathLayer].
 */

struct _AtrebasFeatureLayer
{
  ShumateLayer    parent_instance;

  AtrebasFeature *feature;
  AtrebasPoint   *coordinates;
  unsigned int    n_points;

  double          border[BORDER_NUM];
  GdkRGBA         fill_color;
  GdkRGBA         stroke_color;
  double          stroke_width;
};

G_DEFINE_TYPE (AtrebasFeatureLayer, atrebas_feature_layer, SHUMATE_TYPE_LAYER);


enum {
  PROP_0,
  PROP_FEATURE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * AtrebasFeatureLayer
 */
static void
atrebas_feature_layer_set_feature (AtrebasFeatureLayer *self,
                               AtrebasFeature      *feature)
{
  AtrebasMapTheme theme;
  JsonArray *coordinates;
  JsonArray *polygon;

  g_assert (ATREBAS_IS_FEATURE_LAYER (self));
  g_return_if_fail (ATREBAS_IS_FEATURE (feature));

  self->feature = g_object_ref (feature);

  /* Get the fill and stroke from the #AtrebasFeature */
  gdk_rgba_parse (&self->fill_color, atrebas_feature_get_color (feature));
  gdk_rgba_parse (&self->stroke_color, atrebas_feature_get_color (feature));
  self->fill_color.alpha = 0.25;

  /* Languages have a dotted border (ie. cultural influence) */
  theme = atrebas_feature_get_theme (feature);

  if (theme == ATREBAS_MAP_THEME_LANGUAGE)
    {
      self->border[BORDER_ON] = 3.0;
      self->border[BORDER_OFF] = 5.0;
      self->stroke_width = 3.0;
    }

  /* Traditional territories have a dashed border */
  else if (theme == ATREBAS_MAP_THEME_TERRITORY)
    {
      self->border[BORDER_ON] = 12.0;
      self->border[BORDER_OFF] = 3.0;
      self->stroke_width = 2.0;
    }

  /* Treatied territories have a solid border */
  else if (theme == ATREBAS_MAP_THEME_TREATY)
    {
      self->border[BORDER_ON] = 1.0;
      self->border[BORDER_OFF] = 0.0;
      self->stroke_width = 2.0;
    }

  /* Preload the coordinates into a plain array */
  coordinates = atrebas_feature_get_coordinates (self->feature);
  polygon = json_array_get_array_element (coordinates, 0);

  self->n_points = json_array_get_length (polygon);
  self->coordinates = g_new (AtrebasPoint, self->n_points);

  for (unsigned int i = 0; i < self->n_points; i++)
    {
      JsonArray *point = json_array_get_array_element (polygon, i);
      double longitude = json_array_get_double_element (point, 0);
      double latitude = json_array_get_double_element (point, 1);

      self->coordinates[i].longitude = longitude;
      self->coordinates[i].latitude = latitude;
    }
}

/*
 * GtkWidget
 */
static void
atrebas_feature_layer_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  AtrebasFeatureLayer *self = (AtrebasFeatureLayer *)widget;
  ShumateViewport *viewport;
  cairo_t *cr;
  int width, height;

  if (!gtk_widget_get_visible (widget) ||
      (width = gtk_widget_get_allocated_width (widget)) <= 0 ||
      (height = gtk_widget_get_allocated_height (widget)) <= 0)
    return;

  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (0, 0, width, height));
  viewport = shumate_layer_get_viewport (SHUMATE_LAYER (self));

  /* Mark out the boundaries of the feature */
  for (unsigned int i = 0; i < self->n_points; i++)
    {
      AtrebasPoint point = self->coordinates[i];
      double x, y;

      shumate_viewport_location_to_widget_coords (viewport,
                                                  widget,
                                                  point.latitude,
                                                  point.longitude,
                                                  &x,
                                                  &y);
      cairo_line_to (cr, x, y);
    }
  cairo_close_path (cr);

  /* Paint the fill and border */
  gdk_cairo_set_source_rgba (cr, &self->fill_color);
  cairo_fill_preserve (cr);

  gdk_cairo_set_source_rgba (cr, &self->stroke_color);
  cairo_set_dash (cr, self->border, BORDER_NUM, 0);
  cairo_set_line_width (cr, self->stroke_width);
  cairo_stroke (cr);

  cairo_destroy (cr);
}


/*
 *  GObject
 */
static void
atrebas_feature_layer_constructed (GObject *object)
{
  AtrebasFeatureLayer *self = ATREBAS_FEATURE_LAYER (object);
  ShumateViewport *viewport;

  G_OBJECT_CLASS (atrebas_feature_layer_parent_class)->constructed (object);

  /* Connect to the ShumateViewport after the parent class is constructed */
  viewport = shumate_layer_get_viewport (SHUMATE_LAYER (self));
  g_signal_connect_object (viewport,
                           "notify::latitude",
                           G_CALLBACK (gtk_widget_queue_draw),
                           GTK_WIDGET (self),
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (viewport,
                           "notify::longitude",
                           G_CALLBACK (gtk_widget_queue_draw),
                           GTK_WIDGET (self),
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (viewport,
                           "notify::rotation",
                           G_CALLBACK (gtk_widget_queue_draw),
                           GTK_WIDGET (self),
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (viewport,
                           "notify::zoom-level",
                           G_CALLBACK (gtk_widget_queue_draw),
                           GTK_WIDGET (self),
                           G_CONNECT_SWAPPED);
}

static void
atrebas_feature_layer_dispose (GObject *object)
{
  AtrebasFeatureLayer *self = ATREBAS_FEATURE_LAYER (object);
  ShumateViewport *viewport = shumate_layer_get_viewport (SHUMATE_LAYER (self));

  g_signal_handlers_disconnect_by_data (viewport, self);

  G_OBJECT_CLASS (atrebas_feature_layer_parent_class)->dispose (object);
}

static void
atrebas_feature_layer_finalize (GObject *object)
{
  AtrebasFeatureLayer *self = ATREBAS_FEATURE_LAYER (object);

  g_clear_pointer (&self->coordinates, g_free);
  g_clear_object (&self->feature);

  G_OBJECT_CLASS (atrebas_feature_layer_parent_class)->finalize (object);
}

static void
atrebas_feature_layer_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  AtrebasFeatureLayer *self = ATREBAS_FEATURE_LAYER (object);

  switch (property_id)
    {
    case PROP_FEATURE:
      g_value_set_object (value, self->feature);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
atrebas_feature_layer_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  AtrebasFeatureLayer *self = ATREBAS_FEATURE_LAYER (object);

  switch (property_id)
    {
    case PROP_FEATURE:
      atrebas_feature_layer_set_feature (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
atrebas_feature_layer_class_init (AtrebasFeatureLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = atrebas_feature_layer_constructed;
  object_class->dispose = atrebas_feature_layer_dispose;
  object_class->finalize = atrebas_feature_layer_finalize;
  object_class->get_property = atrebas_feature_layer_get_property;
  object_class->set_property = atrebas_feature_layer_set_property;

  widget_class->snapshot = atrebas_feature_layer_snapshot;

  /**
   * AtrebasFeatureLayer:feature
   *
   * The #AtrebasFeature the layer represents.
   */
  properties [PROP_FEATURE] =
    g_param_spec_object ("feature",
                         "Feature",
                         "The feature the row represents.",
                         ATREBAS_TYPE_FEATURE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_feature_layer_init (AtrebasFeatureLayer *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self), "atrebas-feature-layer");

  self->border[0] = 0.0;
  self->border[1] = 0.0;
  self->fill_color = (GdkRGBA){0.0, 0.0, 0.0, 0.0};
  self->stroke_color = (GdkRGBA){0.0, 0.0, 0.0, 0.0};
  self->stroke_width = 2.0;
}

/**
 * atrebas_feature_layer_new:
 * @viewport: the #ShumateViewport
 * @feature: a #AtrebasFeature
 *
 * Creates a new #AtrebasFeatureLayer.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
atrebas_feature_layer_new (ShumateViewport *viewport,
                       AtrebasFeature      *feature)
{
  return g_object_new (ATREBAS_TYPE_FEATURE_LAYER,
                       "feature",  feature,
                       "viewport", viewport,
                       NULL);
}

/**
 * atrebas_feature_layer_get_feature:
 * @layer: a #AtrebasFeatureLayer
 *
 * Get the #AtrebasFeature this layer represents.
 *
 * Returns: (transfer none): a #AtrebasFeature
 */
AtrebasFeature *
atrebas_feature_layer_get_feature (AtrebasFeatureLayer *layer)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE_LAYER (layer), NULL);

  return layer->feature;
}

