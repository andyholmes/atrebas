// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-legend-row"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <shumate/shumate.h>

#include "atrebas-bookmarks.h"
#include "atrebas-feature.h"
#include "atrebas-feature-layer.h"
#include "atrebas-legend-symbol.h"
#include "atrebas-legend-row.h"
#include "atrebas-utils.h"


/**
 * SECTION:atrebaslocationrow
 * @short_description: A map legend item
 * @title: AtrebasLegendRow
 * @stability: Unstable
 *
 * #AtrebasLegendRow is a widget used to represent an item in a map legend. It is
 * used by #AtrebasMapView to display and control map features.
 */

struct _AtrebasLegendRow
{
  GtkListBoxRow    parent_instance;

  ShumateLayer    *layer;

  AtrebasLegendSymbol *legend_symbol;
  GtkImage        *theme_image;
  GtkLabel        *name_label;

  GAction         *bookmark_action;
  GtkButton       *bookmark_button;
  GBinding        *layer_binding;
  GtkButton       *layer_button;
};

G_DEFINE_TYPE (AtrebasLegendRow, atrebas_legend_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_LAYER,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
on_active_changed (GtkToggleButton *button,
                   GParamSpec      *pspec,
                   AtrebasLegendRow    *self)
{
  if (gtk_toggle_button_get_active (button))
    {
      gtk_button_set_icon_name (GTK_BUTTON (button), "atrebas-visible-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Hide Layer"));
    }
  else
    {
      gtk_button_set_icon_name (GTK_BUTTON (button), "atrebas-invisible-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Show Layer"));
    }
}


/*
 * GActions
 */
static void
bookmark_change_state (GSimpleAction *action,
                       GVariant      *value,
                       gpointer       user_data)
{
  AtrebasLegendRow *self = ATREBAS_LEGEND_ROW (user_data);
  AtrebasBookmarks *bookmarks = ATREBAS_BOOKMARKS (atrebas_bookmarks_get_default ());
  AtrebasFeature *feature = NULL;

  if (ATREBAS_IS_FEATURE_LAYER (self->layer))
    feature = atrebas_feature_layer_get_feature (ATREBAS_FEATURE_LAYER (self->layer));

  if (feature == NULL)
    return;

  bookmarks = ATREBAS_BOOKMARKS (atrebas_bookmarks_get_default ());

  if (g_variant_get_boolean (value))
    atrebas_bookmarks_add_place (bookmarks, GEOCODE_PLACE (feature));
  else
    atrebas_bookmarks_remove_place (bookmarks, GEOCODE_PLACE (feature));
}

static void
bookmark_update_state (GListModel   *model,
                       unsigned int  position,
                       unsigned int  removed,
                       unsigned int  added,
                       AtrebasLegendRow *self)
{
  AtrebasBookmarks *bookmarks = ATREBAS_BOOKMARKS (model);
  AtrebasFeature *feature = NULL;
  GVariant *value = NULL;

  if (ATREBAS_IS_FEATURE_LAYER (self->layer))
    feature = atrebas_feature_layer_get_feature (ATREBAS_FEATURE_LAYER (self->layer));

  if (feature == NULL)
    return;

  if (atrebas_bookmarks_has_place (bookmarks, GEOCODE_PLACE (feature)))
    value = g_variant_new_boolean (TRUE);
  else
    value = g_variant_new_boolean (FALSE);

  g_simple_action_set_state (G_SIMPLE_ACTION (self->bookmark_action), value);
}

static GActionEntry place_actions[] = {
  {"bookmark", NULL, NULL, "false", bookmark_change_state}
};


/*
 * AtrebasLegendRow
 */
static void
atrebas_legend_row_refresh (AtrebasLegendRow *self)
{
  const char *name = NULL;
  const char *icon_name = NULL;
  const char *icon_text = NULL;
  AtrebasFeature *feature = NULL;

  g_assert (ATREBAS_IS_LEGEND_ROW (self));

  if (ATREBAS_IS_FEATURE_LAYER (self->layer))
    {
      AtrebasMapTheme theme;

      feature = atrebas_feature_layer_get_feature (ATREBAS_FEATURE_LAYER (self->layer));
      theme = atrebas_feature_get_theme (feature);

      name = geocode_place_get_name (GEOCODE_PLACE (feature));
      icon_name = atrebas_map_theme_icon (theme);
      icon_text = atrebas_map_theme_name (theme);
    }

  gtk_label_set_label (self->name_label, name);
  gtk_image_set_from_icon_name (self->theme_image, icon_name);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->theme_image), icon_text);
  atrebas_legend_symbol_set_feature (self->legend_symbol, feature);

  /* Bookmark Button */
  bookmark_update_state (atrebas_bookmarks_get_default (), 0, 0, 0, self);
}


/*
 * GObject
 */
static void
atrebas_legend_row_finalize (GObject *object)
{
  AtrebasLegendRow *self = ATREBAS_LEGEND_ROW (object);

  g_clear_object (&self->layer);

  G_OBJECT_CLASS (atrebas_legend_row_parent_class)->finalize (object);
}

static void
atrebas_legend_row_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  AtrebasLegendRow *self = ATREBAS_LEGEND_ROW (object);

  switch (prop_id)
    {
    case PROP_LAYER:
      g_value_set_object (value, self->layer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_legend_row_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  AtrebasLegendRow *self = ATREBAS_LEGEND_ROW (object);

  switch (prop_id)
    {
    case PROP_LAYER:
      atrebas_legend_row_set_layer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_legend_row_class_init (AtrebasLegendRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = atrebas_legend_row_finalize;
  object_class->get_property = atrebas_legend_row_get_property;
  object_class->set_property = atrebas_legend_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Atrebas/ui/atrebas-legend-row.ui");
  gtk_widget_class_bind_template_child (widget_class, AtrebasLegendRow, bookmark_button);
  gtk_widget_class_bind_template_child (widget_class, AtrebasLegendRow, layer_button);
  gtk_widget_class_bind_template_child (widget_class, AtrebasLegendRow, legend_symbol);
  gtk_widget_class_bind_template_child (widget_class, AtrebasLegendRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, AtrebasLegendRow, theme_image);
  gtk_widget_class_bind_template_callback (widget_class, on_active_changed);

  /**
   * AtrebasLegendRow:layer
   *
   * The map layer representing the feature geometry.
   */
  properties [PROP_LAYER] =
    g_param_spec_object ("layer",
                         "Layer",
                         "The map layer representing the feature geometry.",
                         SHUMATE_TYPE_LAYER,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_legend_row_init (AtrebasLegendRow *self)
{
  g_autoptr (GSimpleActionGroup) group = NULL;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   place_actions,
                                   G_N_ELEMENTS (place_actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "place",
                                  G_ACTION_GROUP (group));

  self->bookmark_action = g_action_map_lookup_action (G_ACTION_MAP (group),
                                                      "bookmark");

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (atrebas_bookmarks_get_default (),
                    "items-changed",
                    G_CALLBACK (bookmark_update_state),
                    self);
  bookmark_update_state (atrebas_bookmarks_get_default (), 0, 0, 0, self);
}

/**
 * atrebas_legend_row_new:
 * @layer: a #ShumateLayer
 *
 * Create a new #AtrebasLegendRow.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
atrebas_legend_row_new (ShumateLayer *layer)
{
  g_return_val_if_fail (SHUMATE_IS_LAYER (layer), NULL);

  return g_object_new (ATREBAS_TYPE_LEGEND_ROW,
                       "layer", layer,
                       NULL);
}

/**
 * atrebas_legend_row_get_layer:
 * @row: a #AtrebasLegendRow
 *
 * Get the #ShumateLayer representing the feature geometry.
 *
 * Returns: (transfer none) (nullable): a #ShumateLayet
 */
ShumateLayer *
atrebas_legend_row_get_layer (AtrebasLegendRow *row)
{
  g_return_val_if_fail (ATREBAS_IS_LEGEND_ROW (row), FALSE);

  return row->layer;
}

/**
 * atrebas_legend_row_set_layer:
 * @row: a #AtrebasLegendRow
 * @layer: (nullable): a #ShumateLayer
 *
 * Set the #ShumateLayer the row represents.
 */
void
atrebas_legend_row_set_layer (AtrebasLegendRow *row,
                          ShumateLayer *layer)
{
  g_return_if_fail (ATREBAS_IS_LEGEND_ROW (row));
  g_return_if_fail (layer == NULL || SHUMATE_IS_LAYER (layer));

  if (!g_set_object (&row->layer, layer))
    return;

  if (G_IS_BINDING (row->layer_binding))
    {
      g_binding_unbind (row->layer_binding);
      g_clear_object (&row->layer_binding);
    }

  if (ATREBAS_IS_FEATURE_LAYER (row->layer))
    {
      row->layer_binding = g_object_bind_property (row->layer,        "sensitive",
                                                   row->layer_button, "active",
                                                   (G_BINDING_BIDIRECTIONAL |
                                                    G_BINDING_SYNC_CREATE));
      g_object_ref (row->layer_binding);
    }

  atrebas_legend_row_refresh (row);
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_LAYER]);
}

