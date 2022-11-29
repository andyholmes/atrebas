// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-place-bar"

#include "config.h"

#include <geocode-glib/geocode-glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "atrebas-bookmarks.h"
#include "atrebas-feature.h"
#include "atrebas-place-bar.h"
#include "atrebas-place-header.h"
#include "atrebas-search-model.h"
#include "atrebas-utils.h"


/**
 * SECTION:atrebasplacebar
 * @short_description: A place bar
 * @title: AtrebasPlaceBar
 * @stability: Unstable
 *
 * #AtrebasPlaceBar is a widget meant for use in a marker popup or where a short
 * location description is needed.
 */

struct _AtrebasPlaceBar
{
  GtkBox        parent_instance;

  GeocodePlace *place;
  GListModel   *features;
  double        latitude;
  double        longitude;

  /* Template Widgets */
  GtkStack     *stack;
  GtkLabel     *error_label;

  GtkRevealer  *revealer;
  GtkWidget    *culture_section;
  GtkLabel     *language_label;
  GtkLabel     *treaty_label;
  GtkLabel     *information_label;
  GtkWidget    *address_section;
  GtkLabel     *address_label;
};

G_DEFINE_TYPE (AtrebasPlaceBar, atrebas_place_bar, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_FEATURES,
  PROP_LATITUDE,
  PROP_LONGITUDE,
  PROP_PLACE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
on_info_activated (GtkGestureClick *gesture,
                   int              n_press,
                   double           x,
                   double           y,
                   AtrebasPlaceBar     *self)
{
  gboolean current;

  g_assert (ATREBAS_IS_PLACE_BAR (self));

  current = gtk_revealer_get_reveal_child (self->revealer);
  gtk_revealer_set_reveal_child (self->revealer, !current);
}

static void
atrebas_place_bar_refresh (AtrebasPlaceBar *self)
{
  const char *name = NULL;
  g_autofree char *address = NULL;
  g_autofree char *information = NULL;

  if (GEOCODE_IS_PLACE (self->place))
    {
      const char *street = geocode_place_get_street_address (self->place);
      const char *city = geocode_place_get_town (self->place);
      const char *postal = geocode_place_get_postal_code (self->place);
      const char *state = geocode_place_get_state (self->place);
      const char *country = geocode_place_get_country (self->place);

      name = geocode_place_get_name (self->place);

      if (ATREBAS_IS_FEATURE (self->place))
        {
          AtrebasFeature *feature = ATREBAS_FEATURE (self->place);

          information = g_strdup_printf ("<a href=\"%s\">Native Land Digital</a>",
                                         atrebas_feature_get_uri (feature));
        }
      else
        {
          if (name && street && city && postal && state && country)
            address = g_strdup_printf ("<b>%s</b>\n%s\n%s %s\n%s, %s",
                                       name,
                                       street,
                                       city, postal,
                                       state, country);
          else
            {
              GeocodeLocation *location;
              double latitude, longitude;

              location = geocode_place_get_location (self->place);
              latitude = geocode_location_get_latitude (location);
              longitude = geocode_location_get_longitude (location);
              address = g_strdup_printf ("%lf, %lf", latitude, longitude);
            }
        }
    }

  gtk_widget_set_visible (self->address_section, !!address);
  gtk_label_set_label (self->address_label, address);

  if (information == NULL)
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (GTK_WIDGET (self->information_label));
      gtk_style_context_add_class (context, "atrebas-subtitle");
      information = g_strdup (_("Not available"));
    }
  else
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (GTK_WIDGET (self->information_label));
      gtk_style_context_remove_class (context, "atrebas-subtitle");
    }

  gtk_label_set_label (self->information_label, information);

  if (self->place)
    gtk_stack_set_visible_child_name (self->stack, "info");
}

static void
reverse_resolve_cb (GeocodeReverse *reverse,
                    GAsyncResult   *result,
                    AtrebasPlaceBar   *self)
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
   * atrebas_place_bar_set_place(), so we do this manually */
  if (g_set_object (&self->place, place))
    {
      atrebas_place_bar_refresh (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PLACE]);
    }
}

static void
atrebas_place_bar_resolve (AtrebasPlaceBar *self)
{
  g_assert (ATREBAS_IS_PLACE_BAR (self));

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

      if (self->latitude != place_latitude || self->longitude != place_longitude)
        g_clear_object (&self->place);
    }

  if (self->place == NULL)
    {
      g_autoptr (GeocodeLocation) location = NULL;
      g_autoptr (GeocodeReverse) reverse = NULL;

      /* Create a query for the present location of the marker */
      location = geocode_location_new (self->latitude, self->longitude, 100.0);
      reverse = geocode_reverse_new_for_location (location);

      /* Start the search and switch to the loading page */
      geocode_reverse_resolve_async (reverse,
                                     NULL,
                                     (GAsyncReadyCallback)reverse_resolve_cb,
                                     self);
      gtk_stack_set_visible_child_name (self->stack, "load");
    }

  atrebas_place_bar_refresh (self);
}

static void
on_items_changed (GListModel   *model,
                  unsigned int  position,
                  unsigned int  removed,
                  unsigned int  added,
                  AtrebasPlaceBar  *self)
{
  g_autoptr (GString) languages = NULL;
  g_autoptr (GString) treaties = NULL;
  unsigned int n_features;

  /* Add new features */
  languages = g_string_new ("");
  treaties = g_string_new ("");

  n_features = g_list_model_get_n_items (model);

  for (unsigned int i = 0; i < n_features; i++)
    {
      g_autoptr (AtrebasFeature) feature = g_list_model_get_item (model, i);
      const char *name = geocode_place_get_name (GEOCODE_PLACE (feature));
      const char *uri = atrebas_feature_get_uri (feature);

      /* If this feature is a language, add it to the list */
      if (atrebas_feature_get_theme (feature) == ATREBAS_MAP_THEME_LANGUAGE)
        {
          if (languages->len > 0)
            g_string_append_c (languages, '\n');

          g_string_append_printf (languages, "<a href=\"%s\">%s</a>", uri, name);
        }
      else if (atrebas_feature_get_theme (feature) == ATREBAS_MAP_THEME_TREATY)
        {
          if (treaties->len > 0)
            g_string_append_c (treaties, '\n');

          g_string_append_printf (treaties, "<a href=\"%s\">%s</a>", uri, name);
        }
    }

  if (languages->len > 0)
    gtk_label_set_label (self->language_label, languages->str);
  else
    gtk_label_set_label (self->treaty_label, _("Unknown"));

  if (treaties->len > 0)
    gtk_label_set_label (self->treaty_label, treaties->str);
  else
    gtk_label_set_label (self->treaty_label, _("Unknown"));

  atrebas_place_bar_resolve (self);
}


/*
 * GActions
 */
static void
bookmark_action (GtkWidget  *widget,
                 const char *action_name,
                 GVariant   *parameter)
{
  AtrebasPlaceBar *self = ATREBAS_PLACE_BAR (widget);
  AtrebasBookmarks *bookmarks = ATREBAS_BOOKMARKS (atrebas_bookmarks_get_default ());

  if (GEOCODE_IS_PLACE (self->place))
    {
      if (atrebas_bookmarks_has_place (bookmarks, self->place))
        atrebas_bookmarks_remove_place (bookmarks, self->place);
      else
        atrebas_bookmarks_add_place (bookmarks, self->place);
    }

  atrebas_place_bar_refresh (self);
}

static void
update_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *parameter)
{
  AtrebasPlaceBar *self = ATREBAS_PLACE_BAR (widget);

  g_assert (ATREBAS_IS_PLACE_BAR (self));

  atrebas_place_bar_resolve (self);
}


/*
 * GObject
 */
static void
atrebas_place_bar_finalize (GObject *object)
{
  AtrebasPlaceBar *self = ATREBAS_PLACE_BAR (object);

  g_clear_object (&self->place);
  g_clear_object (&self->features);

  G_OBJECT_CLASS (atrebas_place_bar_parent_class)->finalize (object);
}

static void
atrebas_place_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  AtrebasPlaceBar *self = ATREBAS_PLACE_BAR (object);

  switch (prop_id)
    {
    case PROP_FEATURES:
      g_value_set_object (value, self->features);
      break;

    case PROP_LATITUDE:
      g_value_set_double (value, self->latitude);
      break;

    case PROP_LONGITUDE:
      g_value_set_double (value, self->longitude);
      break;

    case PROP_PLACE:
      g_value_set_object (value, self->place);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_place_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  AtrebasPlaceBar *self = ATREBAS_PLACE_BAR (object);

  switch (prop_id)
    {
    case PROP_LATITUDE:
      atrebas_place_bar_set_latitude (self, g_value_get_double (value));
      break;

    case PROP_LONGITUDE:
      atrebas_place_bar_set_longitude (self, g_value_get_double (value));
      break;

    case PROP_PLACE:
      atrebas_place_bar_set_place (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_place_bar_class_init (AtrebasPlaceBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = atrebas_place_bar_finalize;
  object_class->get_property = atrebas_place_bar_get_property;
  object_class->set_property = atrebas_place_bar_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Atrebas/ui/atrebas-place-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceBar, stack);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceBar, revealer);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceBar, culture_section);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceBar, language_label);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceBar, treaty_label);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceBar, information_label);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceBar, address_section);
  gtk_widget_class_bind_template_child (widget_class, AtrebasPlaceBar, address_label);
  gtk_widget_class_bind_template_callback (widget_class, on_info_activated);
  gtk_widget_class_install_action (widget_class, "place.bookmark", NULL, bookmark_action);
  gtk_widget_class_install_action (widget_class, "place.update", NULL, update_action);

  /**
   * AtrebasPlaceBar:place
   *
   * The #GeocodePlace the color represents.
   */
  properties [PROP_FEATURES] =
    g_param_spec_object ("features",
                         "Features",
                         "The features that intersect with the centroid.",
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasPlaceBar:latitude:
   *
   * The north–south position of a point.
   */
  properties [PROP_LATITUDE] =
    g_param_spec_double ("latitude",
                         "Latitude",
                         "The north–south position of a point.",
                         ATREBAS_MIN_LATITUDE, ATREBAS_MAX_LATITUDE,
                         0.0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasPlaceBar:longitude:
   *
   * The east-west position of a point.
   */
  properties [PROP_LONGITUDE] =
    g_param_spec_double ("longitude",
                         "Longitude",
                         "The east-west position of a point.",
                         ATREBAS_MIN_LONGITUDE, ATREBAS_MAX_LONGITUDE,
                         0.0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasPlaceBar:place
   *
   * The #GeocodePlace the color represents.
   */
  properties [PROP_PLACE] =
    g_param_spec_object ("place",
                         "Place",
                         "The place the bar represents.",
                         GEOCODE_TYPE_PLACE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_place_bar_init (AtrebasPlaceBar *self)
{
  self->features = atrebas_search_model_new (NULL);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->features,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    self);
  g_object_bind_property (self,           "latitude",
                          self->features, "latitude",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self,           "longitude",
                          self->features, "longitude",
                          G_BINDING_SYNC_CREATE);
}

/**
 * atrebas_place_bar_new:
 * @place: (nullable): a #GeocodePlace
 *
 * Create a new #AtrebasPlaceBar.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
atrebas_place_bar_new (GeocodePlace *place)
{
  return g_object_new (ATREBAS_TYPE_PLACE_BAR,
                       "place", place,
                       NULL);
}

/**
 * atrebas_place_bar_get_features:
 * @bar: a #AtrebasPlaceBar
 *
 * Get the #GListModel of objects providing cultural data for @bar.
 *
 * Returns: (transfer none) (nullable): a #GListModel
 */
GListModel *
atrebas_place_bar_get_features (AtrebasPlaceBar *bar)
{
  g_return_val_if_fail (ATREBAS_IS_PLACE_BAR (bar), NULL);

  return bar->features;
}

/**
 * atrebas_place_bar_get_latitude:
 * @bar: a #AtrebasPlaceBar
 *
 * Get the north-south position of @bar.
 *
 * Returns: a latitude
 */
double
atrebas_place_bar_get_latitude (AtrebasPlaceBar *bar)
{
  g_return_val_if_fail (ATREBAS_IS_PLACE_BAR (bar), 0.0);

  return bar->latitude;
}

/**
 * atrebas_place_bar_set_latitude:
 * @bar_bar: a #AtrebasPlaceBar
 * @latitude: a latitude
 *
 * Set the north-south position of @bar to @latitude.
 */
void
atrebas_place_bar_set_latitude (AtrebasPlaceBar *bar,
                            double       latitude)
{
  g_return_if_fail (ATREBAS_IS_PLACE_BAR (bar));
  g_return_if_fail (latitude >= ATREBAS_MIN_LATITUDE && latitude <= ATREBAS_MAX_LATITUDE);

  if (bar->latitude == latitude)
    return;

  bar->latitude = latitude;
  g_object_notify_by_pspec (G_OBJECT (bar), properties [PROP_LATITUDE]);

  gtk_stack_set_visible_child_name (bar->stack, "load");
}

/**
 * atrebas_place_bar_get_longitude:
 * @bar: a #AtrebasPlaceBar
 *
 * Get the east-west position of @bar.
 *
 * Returns: a longitude
 */
double
atrebas_place_bar_get_longitude (AtrebasPlaceBar *bar)
{
  g_return_val_if_fail (ATREBAS_IS_PLACE_BAR (bar), 0.0);

  return bar->longitude;
}

/**
 * atrebas_place_bar_set_longitude:
 * @bar: a #AtrebasPlaceBar
 * @longitude: a longitude
 *
 * Set the east-west of @bar to @longitude.
 */
void
atrebas_place_bar_set_longitude (AtrebasPlaceBar *bar,
                            double      longitude)
{
  g_return_if_fail (ATREBAS_IS_PLACE_BAR (bar));
  g_return_if_fail (longitude >= ATREBAS_MIN_LONGITUDE && longitude <= ATREBAS_MAX_LONGITUDE);

  if (bar->longitude == longitude)
    return;

  bar->longitude = longitude;
  g_object_notify_by_pspec (G_OBJECT (bar), properties [PROP_LONGITUDE]);

  gtk_stack_set_visible_child_name (bar->stack, "load");
}

/**
 * atrebas_place_bar_get_place:
 * @bar: a #AtrebasPlaceBar
 *
 * Get the #GeocodePlace represented by @bar.
 *
 * Returns: (transfer none) (nullable): a #GeocodePlace
 */
GeocodePlace *
atrebas_place_bar_get_place (AtrebasPlaceBar *bar)
{
  g_return_val_if_fail (ATREBAS_IS_PLACE_BAR (bar), NULL);

  return bar->place;
}

/**
 * atrebas_place_bar_set_place:
 * @bar: a #AtrebasPlaceBar
 * @place: (nullable): a #GeocodePlace
 *
 * Set the #GeocodePlace represented by @bar.
 */
void
atrebas_place_bar_set_place (AtrebasPlaceBar  *bar,
                         GeocodePlace *place)
{
  g_return_if_fail (ATREBAS_IS_PLACE_BAR (bar));

  if (!g_set_object (&bar->place, place))
    return;

  if (GEOCODE_IS_PLACE (bar->place))
    {
      GeocodeLocation *location = geocode_place_get_location (bar->place);

      g_object_set (bar,
                    "latitude",  geocode_location_get_latitude (location),
                    "longitude", geocode_location_get_longitude (location),
                    NULL);
    }

  atrebas_place_bar_refresh (bar);
  g_object_notify_by_pspec (G_OBJECT (bar), properties [PROP_PLACE]);
}

