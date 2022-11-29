// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-legend"

#include "config.h"

#include <geocode-glib/geocode-glib.h>
#include <gtk/gtk.h>

#include "atrebas-legend.h"
#include "atrebas-legend-row.h"


/**
 * SECTION:atrebaslegend
 * @short_description: A map legend
 * @title: AtrebasLegend
 * @stability: Unstable
 *
 * #AtrebasLegend displays a list of #AtrebasFeatureLayer objects with controls.
 */

struct _AtrebasLegend
{
  GtkBox      parent_instance;

  GListModel *layers;
  GtkListBox *layer_list;
};

G_DEFINE_TYPE (AtrebasLegend, atrebas_legend, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_LAYERS,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * AtrebasLegend
 */
static GtkWidget *
atrebas_legend_create_row (gpointer item,
                       gpointer user_data)
{
  ShumateLayer *layer = SHUMATE_LAYER (item);

  g_assert (SHUMATE_IS_LAYER (layer));

  return g_object_new (ATREBAS_TYPE_LEGEND_ROW,
                       "layer", layer,
                       NULL);
}


/*
 * GObject
 */
static void
atrebas_legend_finalize (GObject *object)
{
  AtrebasLegend *self = ATREBAS_LEGEND (object);

  g_clear_object (&self->layers);

  G_OBJECT_CLASS (atrebas_legend_parent_class)->finalize (object);
}

static void
atrebas_legend_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  AtrebasLegend *self = ATREBAS_LEGEND (object);

  switch (prop_id)
    {
    case PROP_LAYERS:
      g_value_set_object (value, self->layers);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_legend_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  AtrebasLegend *self = ATREBAS_LEGEND (object);

  switch (prop_id)
    {
    case PROP_LAYERS:
      atrebas_legend_set_layers (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_legend_class_init (AtrebasLegendClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = atrebas_legend_finalize;
  object_class->get_property = atrebas_legend_get_property;
  object_class->set_property = atrebas_legend_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Atrebas/ui/atrebas-legend.ui");
  gtk_widget_class_bind_template_child (widget_class, AtrebasLegend, layer_list);

  /**
   * AtrebasLegend:layers
   *
   * The #GListModel of #ShumateLayers the legend controls.
   */
  properties [PROP_LAYERS] =
    g_param_spec_object ("layers",
                         "Layers",
                         "The list of layers the legend controls.",
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_legend_init (AtrebasLegend *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * atrebas_legend_new:
 * @layers: (nullable): a #GListModel
 *
 * Create a new #AtrebasLegend.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
atrebas_legend_new (GListModel *layers)
{
  return g_object_new (ATREBAS_TYPE_LEGEND,
                       "layers", layers,
                       NULL);
}

/**
 * atrebas_legend_get_map:
 * @legend: a #AtrebasLegend
 *
 * Get the #GeocodePlace represented by @legend.
 *
 * Returns: (transfer none) (nullable): a #GeocodePlace
 */
GListModel *
atrebas_legend_get_layers (AtrebasLegend *legend)
{
  g_return_val_if_fail (ATREBAS_IS_LEGEND (legend), NULL);

  return legend->layers;
}

/**
 * atrebas_legend_set_map:
 * @legend: a #AtrebasLegend
 * @layers: (nullable): a #GListModel of #ShumateLayer
 *
 * Set the #GListModel providing #ShumateLayer objects for @legend.
 */
void
atrebas_legend_set_layers (AtrebasLegend  *self,
                       GListModel *layers)
{
  g_return_if_fail (ATREBAS_IS_LEGEND (self));
  g_return_if_fail (layers == NULL || G_IS_LIST_MODEL (layers));

  if (!g_set_object (&self->layers, layers))
    return;

  gtk_list_box_bind_model (self->layer_list,
                           self->layers,
                           atrebas_legend_create_row,
                           NULL, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAYERS]);
}

