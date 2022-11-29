// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-legend-symbol"

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <math.h>

#include "atrebas-feature.h"
#include "atrebas-legend-symbol.h"


/**
 * SECTION:atrebaslegendsymbol
 * @short_description: A map legend item
 * @title: AtrebasLegendSymbol
 * @stability: Unstable
 *
 * #AtrebasLegendSymbol is a widget meant for use in a map legend. It is used by
 * #AtrebasLegendRow to control the visibility of the associated feature boundary.
 */

struct _AtrebasLegendSymbol
{
  GtkWidget       parent_instance;

  AtrebasFeature *feature;
  GdkRGBA         color;
  GdkRGBA         border;
};

G_DEFINE_TYPE (AtrebasLegendSymbol, atrebas_legend_symbol, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_FEATURE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/**
 * _gdk_rgba_relative_luminance:
 * @rgba: a #GdkRGBA
 *
 * Get the relative luminance of a RGB set
 *
 * See: https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
 *
 * Returns: a relative luminance
 */
static inline double
_gdk_rgba_relative_luminance (GdkRGBA *rgba)
{
  double red = rgba->red;
  double green = rgba->green;
  double blue = rgba->blue;
  double R, G, B;

  R = (red > 0.03928) ? red / 12.92 : pow (((red + 0.055) / 1.055), 2.4);
  G = (green > 0.03928) ? green / 12.92 : pow (((green + 0.055) / 1.055), 2.4);
  B = (blue > 0.03928) ? blue / 12.92 : pow (((blue + 0.055) / 1.055), 2.4);

  return 0.2126 * R + 0.7152 * G + 0.0722 * B;
}


/**
 * _gdk_rgba_contrast:
 * @rgba: a #GdkRGBA
 * @contrast: (out): the #GdkRGBA to fill in
 *
 * Set @contrast that contrasts with @rgba.
 *
 * See: https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef
 */
static inline void
_gdk_rgba_contrast (GdkRGBA *rgba,
                    GdkRGBA *contrast)
{
  double luminance = _gdk_rgba_relative_luminance (rgba);
  double light, dark, value;

  light = (0.07275541795665634 + 0.05) / (luminance + 0.05);
  dark = (luminance + 0.05) / (0.0046439628482972135 + 0.05);
  value = (dark > light) ? 0.06 : 0.94;

  contrast->red = value;
  contrast->green = value;
  contrast->blue = value;
  contrast->alpha = 0.25;
}


/*
 * GtkWidget
 */
static void
atrebas_legend_symbol_snapshot (GtkWidget   *widget,
                                GtkSnapshot *snapshot)
{
  AtrebasLegendSymbol *self = ATREBAS_LEGEND_SYMBOL (widget);
  GtkWidget *child = gtk_widget_get_first_child (widget);
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);
  float radius = width / 2.0;
  GskRoundedRect *border;

  /* Clip to a circle */
  border = gsk_rounded_rect_init (&GSK_ROUNDED_RECT_INIT (0, 0, width, height),
                                  &GRAPHENE_RECT_INIT (0, 0, width, height),
                                  &GRAPHENE_SIZE_INIT (radius, radius),
                                  &GRAPHENE_SIZE_INIT (radius, radius),
                                  &GRAPHENE_SIZE_INIT (radius, radius),
                                  &GRAPHENE_SIZE_INIT (radius, radius));
  gtk_snapshot_push_rounded_clip (snapshot, border);

  /* Fill the circle the draw a border that contrasts with the fill */
  gtk_snapshot_append_color (snapshot, &self->color,
                             &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_append_border (snapshot, border,
                              (float[4]){1.0, 1.0, 1.0, 1.0},
                              (GdkRGBA[4]){
                                self->border,
                                self->border,
                                self->border,
                                self->border,
                              });

  /* Pop and propagate */
  gtk_snapshot_pop (snapshot);

  if (GTK_IS_WIDGET (child))
    gtk_widget_snapshot_child (widget, child, snapshot);
}


/*
 * GObject
 */
static void
atrebas_legend_symbol_finalize (GObject *object)
{
  AtrebasLegendSymbol *self = ATREBAS_LEGEND_SYMBOL (object);

  g_clear_object (&self->feature);

  G_OBJECT_CLASS (atrebas_legend_symbol_parent_class)->finalize (object);
}

static void
atrebas_legend_symbol_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  AtrebasLegendSymbol *self = ATREBAS_LEGEND_SYMBOL (object);

  switch (prop_id)
    {
    case PROP_FEATURE:
      g_value_set_object (value, self->feature);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_legend_symbol_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  AtrebasLegendSymbol *self = ATREBAS_LEGEND_SYMBOL (object);

  switch (prop_id)
    {
    case PROP_FEATURE:
      atrebas_legend_symbol_set_feature (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_legend_symbol_class_init (AtrebasLegendSymbolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = atrebas_legend_symbol_finalize;
  object_class->get_property = atrebas_legend_symbol_get_property;
  object_class->set_property = atrebas_legend_symbol_set_property;

  widget_class->snapshot = atrebas_legend_symbol_snapshot;

  /**
   * AtrebasLegendSymbol:feature
   *
   * The #AtrebasFeature the color represents.
   */
  properties [PROP_FEATURE] =
    g_param_spec_object ("feature",
                         "Feature",
                         "The feature the button represents.",
                         ATREBAS_TYPE_FEATURE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_legend_symbol_init (AtrebasLegendSymbol *self)
{
}

/**
 * atrebas_legend_symbol_new:
 * @feature: (nullable): a #AtrebasFeature
 *
 * Create a new #AtrebasLegendSymbol.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
atrebas_legend_symbol_new (AtrebasFeature *feature)
{
  return g_object_new (ATREBAS_TYPE_LEGEND_SYMBOL,
                       "feature", feature,
                       NULL);
}

/**
 * atrebas_legend_symbol_get_feature:
 * @button: a #AtrebasLegendSymbol
 *
 * Get the #AtrebasFeature represented by @button.
 *
 * Returns: (transfer none) (nullable): a #AtrebasFeature
 */
AtrebasFeature *
atrebas_legend_symbol_get_feature (AtrebasLegendSymbol *button)
{
  g_return_val_if_fail (ATREBAS_IS_LEGEND_SYMBOL (button), NULL);

  return button->feature;
}

/**
 * atrebas_legend_symbol_set_feature:
 * @button: a #AtrebasLegendSymbol
 * @feature: (nullable): a #AtrebasFeature
 *
 * Set the #AtrebasFeature represented by @button.
 */
void
atrebas_legend_symbol_set_feature (AtrebasLegendSymbol *button,
                                   AtrebasFeature      *feature)
{
  g_return_if_fail (ATREBAS_IS_LEGEND_SYMBOL (button));

  if (!g_set_object (&button->feature, feature))
    return;

  if (button->feature != NULL)
    {
      const char *hex;

      hex = atrebas_feature_get_color (button->feature);
      gdk_rgba_parse (&button->color, hex);
      _gdk_rgba_contrast (&button->color, &button->border);
    }
  else
    {
      button->color.red = 0.0;
      button->color.green = 0.0;
      button->color.blue = 0.0;
      button->color.alpha = 0.0;
    }

  g_object_notify_by_pspec (G_OBJECT (button), properties [PROP_FEATURE]);
}

