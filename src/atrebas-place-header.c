// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-place-header"

#include "config.h"

#include <geocode-glib/geocode-glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "atrebas-bookmarks.h"
#include "atrebas-feature.h"
#include "atrebas-place-header.h"
#include "atrebas-utils.h"


/**
 * SECTION:atrebasplaceheader
 * @short_description: A place header
 * @title: AtrebasPlaceHeader
 * @stability: Unstable
 *
 * #AtrebasPlaceHeader is a widget used to display a #GeocodePlace, with actions.
 */

struct _AtrebasPlaceHeader
{
  GtkBox        parent_instance;

  GeocodePlace *place;

  GtkImage     *icon_image;
  GtkLabel     *title_label;
  GtkLabel     *subtitle_label;

  GAction      *bookmark_action;
  GtkButton    *bookmark_button;
  GtkButton    *locate_button;
};

G_DEFINE_TYPE (AtrebasPlaceHeader, atrebas_place_header, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_PLACE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GActions
 */
static void
bookmark_change_state (GSimpleAction *action,
                       GVariant      *value,
                       gpointer       user_data)
{
  AtrebasPlaceHeader *self = ATREBAS_PLACE_HEADER (user_data);
  AtrebasBookmarks *bookmarks;

  if (self->place == NULL)
    return;

  bookmarks = ATREBAS_BOOKMARKS (atrebas_bookmarks_get_default ());

  if (g_variant_get_boolean (value))
    atrebas_bookmarks_add_place (bookmarks, self->place);
  else
    atrebas_bookmarks_remove_place (bookmarks, self->place);
}

static void
bookmark_update_state (GListModel     *model,
                       unsigned int    position,
                       unsigned int    removed,
                       unsigned int    added,
                       AtrebasPlaceHeader *self)
{
  AtrebasBookmarks *bookmarks = ATREBAS_BOOKMARKS (model);
  GVariant *value;

  if (self->place == NULL)
    return;

  if (atrebas_bookmarks_has_place (bookmarks, self->place))
    value = g_variant_new_boolean (TRUE);
  else
    value = g_variant_new_boolean (FALSE);

  g_simple_action_set_state (G_SIMPLE_ACTION (self->bookmark_action), value);
}

static GActionEntry place_actions[] = {
  {"bookmark", NULL, NULL, "false", bookmark_change_state}
};


/*
 * AtrebasPlaceHeader
 */
static void
atrebas_place_header_refresh (AtrebasPlaceHeader *header)
{
  const char *name = _("Unknown");
  const char *icon_name = "atrebas-location-symbolic";
  g_autofree char *description = NULL;
  g_autofree char *address = NULL;
  g_autofree char *information = NULL;

  if (GEOCODE_IS_PLACE (header->place))
    {
      const char *street = geocode_place_get_street_address (header->place);
      const char *city = geocode_place_get_town (header->place);
      const char *postal = geocode_place_get_postal_code (header->place);
      const char *state = geocode_place_get_state (header->place);
      const char *country = geocode_place_get_country (header->place);

      name = geocode_place_get_name (header->place);

      if (ATREBAS_IS_FEATURE (header->place))
        {
          AtrebasFeature *feature = ATREBAS_FEATURE (header->place);
          AtrebasMapTheme theme = atrebas_feature_get_theme (feature);

          icon_name = atrebas_map_theme_icon (theme);
          description = g_strdup (atrebas_map_theme_name (theme));
          information = g_strdup_printf ("<a href=\"%s\">Native Land Digital</a>",
                                         atrebas_feature_get_uri (feature));
        }
      else
        {
          if (city && state && country)
            description = g_strdup_printf ("%s, %s, %s", city, state, country);

          if (name && street && city && postal && state && country)
            {
              address = g_strdup_printf ("<b>%s</b>\n%s\n%s %s\n%s, %s",
                                         name,
                                         street,
                                         city, postal,
                                         state, country);
            }
          else
            {
              GeocodeLocation *location;
              double latitude, longitude;

              location = geocode_place_get_location (header->place);
              latitude = geocode_location_get_latitude (location);
              longitude = geocode_location_get_longitude (location);
              address = g_strdup_printf ("%lf, %lf", latitude, longitude);
            }
        }
    }

  /* ... */
  gtk_image_set_from_icon_name (header->icon_image, icon_name);
  gtk_label_set_label (header->title_label, name);

  gtk_widget_set_visible (GTK_WIDGET (header->subtitle_label), !!description);
  gtk_label_set_label (header->subtitle_label, description);

  /* Bookmark Button */
  bookmark_update_state (atrebas_bookmarks_get_default (), 0, 0, 0, header);
}


/*
 * GObject
 */
static void
atrebas_place_header_finalize (GObject *object)
{
  AtrebasPlaceHeader *self = ATREBAS_PLACE_HEADER (object);

  g_signal_handlers_disconnect_by_data (atrebas_bookmarks_get_default (), self);
  g_clear_object (&self->place);

  G_OBJECT_CLASS (atrebas_place_header_parent_class)->finalize (object);
}

static void
atrebas_place_header_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  AtrebasPlaceHeader *self = ATREBAS_PLACE_HEADER (object);

  switch (prop_id)
    {
    case PROP_PLACE:
      g_value_set_object (value, atrebas_place_header_get_place (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_place_header_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  AtrebasPlaceHeader *self = ATREBAS_PLACE_HEADER (object);

  switch (prop_id)
    {
    case PROP_PLACE:
      atrebas_place_header_set_place (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_place_header_class_init (AtrebasPlaceHeaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = atrebas_place_header_finalize;
  object_class->get_property = atrebas_place_header_get_property;
  object_class->set_property = atrebas_place_header_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Atrebas/ui/atrebas-place-header.ui");
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceHeader, icon_image);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceHeader, title_label);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceHeader, subtitle_label);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceHeader, bookmark_button);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceHeader, locate_button);

  /**
   * AtrebasPlaceHeader:feature
   *
   * The #GeocodePlace the header represents.
   */
  properties [PROP_PLACE] =
    g_param_spec_object ("place",
                         "Place",
                         "The place the header represents.",
                         GEOCODE_TYPE_PLACE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_place_header_init (AtrebasPlaceHeader *self)
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
 * atrebas_place_header_new:
 * @place: (nullable): a #GeocodePlace
 *
 * Create a new #AtrebasPlaceHeader.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
atrebas_place_header_new (GeocodePlace *place)
{
  return g_object_new (ATREBAS_TYPE_PLACE_HEADER,
                       "place", place,
                       NULL);
}

/**
 * atrebas_place_header_get_place:
 * @header: a #AtrebasPlaceHeader
 *
 * Get the #GeocodePlace represented by @header.
 *
 * Returns: (transfer none) (nullable): a #GeocodePlace
 */
GeocodePlace *
atrebas_place_header_get_place (AtrebasPlaceHeader *header)
{
  g_return_val_if_fail (ATREBAS_IS_PLACE_HEADER (header), NULL);

  return header->place;
}

/**
 * atrebas_place_header_set_place:
 * @header: a #AtrebasPlaceHeader
 * @feature: (nullable): a #GeocodePlace
 *
 * Set the #GeocodePlace represented by @header.
 */
void
atrebas_place_header_set_place (AtrebasPlaceHeader *header,
                            GeocodePlace   *place)
{
  g_return_if_fail (ATREBAS_IS_PLACE_HEADER (header));
  g_return_if_fail (place == NULL || GEOCODE_IS_PLACE (place));

  if (!g_set_object (&header->place, place))
    return;

  atrebas_place_header_refresh (header);
  g_object_notify_by_pspec (G_OBJECT (header), properties [PROP_PLACE]);
}

