// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-feature"

#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <math.h>

#include "atrebas-enums.h"
#include "atrebas-feature.h"
#include "atrebas-macros.h"


/**
 * SECTION:atrebasfeature
 * @short_description: A map feature
 * @name: AtrebasFeature
 * @stability: Unstable
 * @include: atrebas.h
 *
 * The #AtrebasFeature class is a subclass of #GeocodePlace representing a map
 * feature of either a language, territory or treaty.
 *
 * Technically these are neither places nor maps, but areas where people
 * traditionally resided, spoke languages and in some cases have negotiated
 * treaties for specific territories.
 *
 * The source of these maps is in GeoJSON format, with each them of map (eg.
 * language) is a GeoJSON `FeatureCollection` and each map is a `Feature` with a
 * `Polygon` geometry. Thus #AtrebasFeature represents a single GeoJSON structure
 * with some additional properties useful for display.
 */

struct _AtrebasFeature
{
  GeocodePlace     parent_instance;

  char            *color;
  JsonArray       *coordinates;
  char            *name_fr;
  char            *nld_id;
  char            *slug;
  AtrebasMapTheme  theme;
  char            *uri;
  char            *uri_fr;
};

G_DEFINE_TYPE (AtrebasFeature, atrebas_feature, GEOCODE_TYPE_PLACE);


enum {
  PROP_0,
  PROP_COLOR,
  PROP_COORDINATES,
  PROP_NAME_FR,
  PROP_NLD_ID,
  PROP_SLUG,
  PROP_THEME,
  PROP_URI,
  PROP_URI_FR,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * AtrebasFeature
 */
static void
atrebas_feature_set_coordinates (AtrebasFeature *self,
                             JsonArray  *coordinates)
{
  g_autoptr (GeocodeBoundingBox) bounds = NULL;
  g_autoptr (GeocodeLocation) location = NULL;
  JsonArray *polygon;
  unsigned int n_points;
  double max_x = 0.0;
  double min_x = 0.0;
  double max_y = 0.0;
  double min_y = 0.0;
  double latitude;
  double longitude;

  g_assert (ATREBAS_IS_FEATURE (self));

  if (self->coordinates == coordinates)
    return;

  g_clear_pointer (&self->coordinates, json_array_unref);

  if (coordinates == NULL)
    return;

  self->coordinates = json_array_ref (coordinates);

  /* Find the bounding box of the outermost ring */
  polygon = json_array_get_array_element (self->coordinates, 0);
  n_points = json_array_get_length (polygon);

  for (unsigned int i = 0; i < n_points; i++)
    {
      JsonArray *point = json_array_get_array_element (polygon, i);
      double x = json_array_get_double_element (point, 0);
      double y = json_array_get_double_element (point, 1);

      if G_UNLIKELY (i == 0)
        {
          min_x = x;
          min_y = y;
          max_x = x;
          max_y = y;
        }
      else
        {
          if (min_x > x)
            min_x = x;

          if (min_y > y)
            min_y = y;

          if (max_x < x)
            max_x = x;

          if (max_y < y)
            max_y = y;
        }
    }

  /* TODO: Take a reasonable center point */
  latitude = min_y + (max_y - min_y) / 2;
  longitude = min_x + (max_x - min_x) / 2;

  /* Transpose to GeocodeBoundingBox and GeocodeLocation */
  bounds = geocode_bounding_box_new (min_y, max_y, min_x, max_x);
  geocode_place_set_bounding_box (GEOCODE_PLACE (self), bounds);

  location = g_object_new (GEOCODE_TYPE_LOCATION,
                           "description", geocode_place_get_name (GEOCODE_PLACE (self)),
                           "latitude",    latitude,
                           "longitude",   longitude,
                           NULL);
  geocode_place_set_location (GEOCODE_PLACE (self), location);
}

/*
 * GObject
 */
static void
atrebas_feature_finalize (GObject *object)
{
  AtrebasFeature *self = ATREBAS_FEATURE (object);

  g_clear_pointer (&self->color, g_free);
  g_clear_pointer (&self->coordinates, json_array_unref);
  g_clear_pointer (&self->name_fr, g_free);
  g_clear_pointer (&self->nld_id, g_free);
  g_clear_pointer (&self->slug, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_pointer (&self->uri_fr, g_free);

  G_OBJECT_CLASS (atrebas_feature_parent_class)->finalize (object);
}

static void
atrebas_feature_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  AtrebasFeature *self = ATREBAS_FEATURE (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      g_value_set_string (value, self->color);
      break;

    case PROP_COORDINATES:
      g_value_set_boxed (value, self->coordinates);
      break;

    case PROP_NAME_FR:
      g_value_set_string (value, self->name_fr);
      break;

    case PROP_NLD_ID:
      g_value_set_string (value, self->nld_id);
      break;

    case PROP_SLUG:
      g_value_set_string (value, self->slug);
      break;

    case PROP_THEME:
      g_value_set_enum (value, self->theme);
      break;

    case PROP_URI:
      g_value_set_string (value, self->uri);
      break;

    case PROP_URI_FR:
      g_value_set_string (value, self->uri_fr);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_feature_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  AtrebasFeature *self = ATREBAS_FEATURE (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      self->color = g_value_dup_string (value);
      break;

    case PROP_COORDINATES:
      atrebas_feature_set_coordinates (self, g_value_get_boxed (value));
      break;

    case PROP_NAME_FR:
      self->name_fr = g_value_dup_string (value);
      break;

    case PROP_NLD_ID:
      self->nld_id = g_value_dup_string (value);
      break;

    case PROP_SLUG:
      self->slug = g_value_dup_string (value);
      break;

    case PROP_THEME:
      self->theme = g_value_get_enum (value);
      break;

    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;

    case PROP_URI_FR:
      self->uri_fr = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_feature_class_init (AtrebasFeatureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = atrebas_feature_finalize;
  object_class->get_property = atrebas_feature_get_property;
  object_class->set_property = atrebas_feature_set_property;

  /**
   * AtrebasFeature:color:
   *
   * The hex color code of the feature.
   */
  properties [PROP_COLOR] =
    g_param_spec_string ("color",
                         "Color",
                         "The hex color code of the feature.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasFeature:coordinates:
   *
   * The bounding coordinates.
   */
  properties [PROP_COORDINATES] =
    g_param_spec_boxed ("coordinates",
                        "Coordinates",
                        "The bounding coordinates.",
                        JSON_TYPE_ARRAY,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasFeature:name-fr:
   *
   * The French name of the feature, if applicable.
   */
  properties [PROP_NAME_FR] =
    g_param_spec_string ("name-fr",
                         "Name (French)",
                         "The French name of the feature, if applicable.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasFeature:nld-id:
   *
   * The ID of the feature.
   */
  properties [PROP_NLD_ID] =
    g_param_spec_string ("nld-id",
                         "NLD ID",
                         "The Native Land Digital ID of the feature.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasFeature:slug:
   *
   * The slug for the feature.
   */
  properties [PROP_SLUG] =
    g_param_spec_string ("slug",
                         "Slug",
                         "The slug for the feature.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasFeature:theme:
   *
   * The theme of the feature.
   */
  properties [PROP_THEME] =
    g_param_spec_enum ("theme",
                       "Theme",
                       "The theme of the feature.",
                       ATREBAS_TYPE_MAP_THEME,
                       ATREBAS_MAP_THEME_TERRITORY,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasFeature:uri:
   *
   * The URI of the feature.
   *
   * This property corresponds with the `description` field of the `properties`
   * object in GeoJSON `Feature` types as returned by <native-land.ca>.
   */
  properties [PROP_URI] =
    g_param_spec_string ("uri",
                         "Description",
                         "The uri of the feature.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasFeature:uri-fr:
   *
   * The French URI of the feature, if applicable.
   *
   * This property corresponds with the `FrenchDescription` field of the
   * `properties` object in GeoJSON `Feature` types as returned by
   * <native-land.ca>.
   */
  properties [PROP_URI_FR] =
    g_param_spec_string ("uri-fr",
                         "Description (French)",
                         "The French uri of the feature, if applicable.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_feature_init (AtrebasFeature *self)
{
  self->theme = ATREBAS_MAP_THEME_TERRITORY;
}

/**
 * atrebas_feature_new:
 * @name: name of the feature
 * @uri: uri of the feature
 * @coordinates: bounding coordinates of the feature
 *
 * Create a new #AtrebasFeature
 *
 * Returns: (transfer full): a #AtrebasFeature
 */
AtrebasFeature *
atrebas_feature_new (const char *name,
                 const char *uri,
                 JsonArray  *coordinates)
{
  g_return_val_if_fail (!atrebas_str_empty0 (name), NULL);
  g_return_val_if_fail (!atrebas_str_empty0 (uri), NULL);
  g_return_val_if_fail (coordinates != NULL, NULL);

  return g_object_new (ATREBAS_TYPE_FEATURE,
                       "name",        name,
                       "uri",         uri,
                       "coordinates", coordinates,
                       NULL);
}

/**
 * atrebas_feature_get_color:
 * @feature: a #AtrebasFeature
 *
 * Get the hex color code for @feature.
 *
 * Returns: (transfer none): a hex color code
 */
const char *
atrebas_feature_get_color (AtrebasFeature *feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), NULL);

  if (feature->color == NULL)
    return "#000000";

  return feature->color;
}

/**
 * atrebas_feature_get_coordinates:
 * @feature: a #AtrebasFeature
 *
 * Get the bounding coordinates of @feature.
 *
 * Returns: (transfer none): a #JsonArray
 */
JsonArray *
atrebas_feature_get_coordinates (AtrebasFeature *feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), NULL);

  return feature->coordinates;
}

/**
 * atrebas_feature_get_name_fr:
 * @feature: a #AtrebasFeature
 *
 * Get the French name of @feature, if applicable.
 *
 * If the feature has no French name, this function will return the English name
 * in its place.
 *
 * Returns: (transfer none): a French name of the feature
 */
const char *
atrebas_feature_get_name_fr (AtrebasFeature *feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), NULL);

  if (feature->name_fr == NULL)
    return geocode_place_get_name (GEOCODE_PLACE (feature));

  return feature->name_fr;
}

/**
 * atrebas_feature_get_nld_id:
 * @feature: a #AtrebasFeature
 *
 * Get the Native Land Digital ID of @feature.
 *
 * Returns: (transfer none): a unique ID
 */
const char *
atrebas_feature_get_nld_id (AtrebasFeature *feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), NULL);

  return feature->nld_id;
}

/**
 * atrebas_feature_get_slug:
 * @feature: a #AtrebasFeature
 *
 * Get the slug for @feature.
 *
 * Returns: (transfer none): a slug
 */
const char *
atrebas_feature_get_slug (AtrebasFeature *feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), NULL);

  return feature->slug;
}

/**
 * atrebas_feature_get_theme:
 * @feature: a #AtrebasFeature
 *
 * Get the theme of @feature.
 *
 * Returns: a #AtrebasMapTheme
 */
AtrebasMapTheme
atrebas_feature_get_theme (AtrebasFeature *feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), ATREBAS_MAP_THEME_TERRITORY);

  return feature->theme;
}

/**
 * atrebas_feature_get_uri:
 * @feature: a #AtrebasFeature
 *
 * Get the URI of @feature.
 *
 * Returns: (transfer none): a URI
 */
const char *
atrebas_feature_get_uri (AtrebasFeature *feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), NULL);

  return feature->uri;
}

/**
 * atrebas_feature_get_uri_fr:
 * @feature: a #AtrebasFeature
 *
 * Get the French-specific URI of @feature, if applicable.
 *
 * If the @feature has no French URI, this function will return the standard URI
 * in its place.
 *
 * Returns: (transfer none): a French uri of the feature
 */
const char *
atrebas_feature_get_uri_fr (AtrebasFeature *feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), NULL);

  if (feature->uri_fr == NULL)
    return feature->uri;

  return feature->uri_fr;
}

/**
 * atrebas_feature_equal:
 * @feature1: (type Atrebas.Feature): a #AtrebasFeature
 * @feature2: (type Atrebas.Feature): a #AtrebasFeature
 *
 * Check whether @feature1 and @feature2 represent the same feature.
 *
 * Returns: %TRUE if equal, %FALSE otherwise
 */
gboolean
atrebas_feature_equal (gconstpointer feature1,
                   gconstpointer feature2)
{
  AtrebasFeature *m1 = (AtrebasFeature *)feature1;
  AtrebasFeature *m2 = (AtrebasFeature *)feature2;

  g_return_val_if_fail (ATREBAS_IS_FEATURE (m1), FALSE);
  g_return_val_if_fail (ATREBAS_IS_FEATURE (m2), FALSE);

  if (feature1 == feature2)
    return TRUE;

  return g_strcmp0 (atrebas_feature_get_nld_id (m1), atrebas_feature_get_nld_id (m2)) == 0;
}

/**
 * atrebas_feature_hash:
 * @feature: (type Atrebas.Feature): a #AtrebasFeature
 *
 * Converts a #AtrebasFeature to a hash value.
 *
 * Returns: a hash value corresponding to @feature
 */
unsigned int
atrebas_feature_hash (gconstpointer feature)
{
  g_return_val_if_fail (ATREBAS_IS_FEATURE ((gpointer)feature), 0);

  return g_str_hash (((AtrebasFeature *)feature)->nld_id);
}

/**
 * atrebas_feature_contains_point:
 * @feature: a #AtrebasFeature
 * @latitude: a north-south position
 * @longitude: an east-west position
 *
 * Check if the point at @latitude and @longitude is inside the boundaries of
 * @feature.
 *
 * Based on "pnpoly":
 *     Copyright 1994-2006 W Randolph Franklin (WRF)
 *     https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html
 *
 * Returns: %TRUE if inside, %FALSE if not
 */
gboolean
atrebas_feature_contains_point (AtrebasFeature *feature,
                            double      latitude,
                            double      longitude)
{
  gboolean ret = FALSE;
  JsonArray *polygon;
  unsigned int n_vertices = 0;

  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), FALSE);
  g_return_val_if_fail (feature->coordinates != NULL, FALSE);

  polygon = json_array_get_array_element (feature->coordinates, 0);
  n_vertices = json_array_get_length (polygon);

  for (unsigned int i = 0, j = n_vertices - 1; i < n_vertices; j = i++)
    {
      JsonArray *next = json_array_get_array_element (polygon, i);
      double nextx = json_array_get_double_element (next, 0);
      double nexty = json_array_get_double_element (next, 1);

      JsonArray *prev = json_array_get_array_element (polygon, j);
      double prevx = json_array_get_double_element (prev, 0);
      double prevy = json_array_get_double_element (prev, 1);

      if ((nexty > latitude) != (prevy > latitude) &&
          (longitude < (prevx - nextx) * (latitude - nexty) / (prevy - nexty) + nextx))
       ret = !ret;
    }

  return ret;
}

/**
 * atrebas_feature_serialize:
 * @feature: a #AtrebasFeature
 *
 * Serialize @feature into a GeoJSON `Feature` node.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
atrebas_feature_serialize (AtrebasFeature *feature)
{
  GeocodePlace *place = GEOCODE_PLACE (feature);
  g_autoptr (JsonBuilder) builder = NULL;
  JsonNode *coordinates;

  g_return_val_if_fail (ATREBAS_IS_FEATURE (feature), NULL);

  builder = json_builder_new ();
  coordinates = json_node_init_array (json_node_alloc (), feature->coordinates);

  /* BEGIN / FeatureCollection / features / [Feature] */
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, "Feature");
  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, feature->nld_id);

  /* BEGIN / FeatureCollection / features / Feature / [properties] */
  json_builder_set_member_name (builder, "properties");
  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "color");
  json_builder_add_string_value (builder, feature->color);
  json_builder_set_member_name (builder, "Name");
  json_builder_add_string_value (builder, geocode_place_get_name (place));
  json_builder_set_member_name (builder, "FrenchName");
  json_builder_add_string_value (builder, feature->name_fr);
  json_builder_set_member_name (builder, "description");
  json_builder_add_string_value (builder, feature->uri);
  json_builder_set_member_name (builder, "FrenchDescription");
  json_builder_add_string_value (builder, feature->uri_fr);
  json_builder_set_member_name (builder, "slug");
  json_builder_add_string_value (builder, feature->slug);
  json_builder_set_member_name (builder, "theme");
  json_builder_add_int_value (builder, feature->theme);

  json_builder_end_object (builder);
  /* END / FeatureCollection / features / Feature / [properties] */

  /* BEGIN / FeatureCollection / features / Feature / [geometry] */
  json_builder_set_member_name (builder, "geometry");
  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, "Polygon");
  json_builder_set_member_name (builder, "coordinates");
  json_builder_add_value (builder, coordinates);

  json_builder_end_object (builder);
  /* END / FeatureCollection / features / Feature / [geometry] */

  json_builder_end_object (builder);
  /* END / FeatureCollection / features / [Feature] */

  return json_builder_get_root (builder);
}

/**
 * atrebas_feature_deserialize:
 * @node: a #JsonNode
 * @error: (nullable): a #GError
 *
 * Deserialize a #AtrebasFeature from @node.
 *
 * Returns: (transfer full): a #AtrebasFeature
 */
AtrebasFeature *
atrebas_feature_deserialize (JsonNode  *node,
                         GError   **error)
{
  const char *type;
  JsonObject *feature;
  JsonObject *properties;
  JsonObject *geometry;
  JsonArray *coordinates;
  const char *color;
  const char *name;
  const char *name_fr;
  const char *nld_id;
  const char *slug;
  const char *uri;
  const char *uri_fr;
  AtrebasMapTheme theme;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (node), NULL);

  /* Try to extract a singular Feature */
  feature = json_node_get_object (node);
  type = json_object_get_string_member (feature, "type");

  if (g_strcmp0 (type, "Feature") != 0)
    {
      g_set_error (error,
                   GEOCODE_ERROR,
                   GEOCODE_ERROR_PARSE,
                   "Unsupported type: %s",
                   type);
      return NULL;
    }

  /* Extract the feature data */
  properties = json_object_get_object_member (feature, "properties");
  color = atrebas_get_string (properties, "color", "#000000");
  name = atrebas_get_string (properties, "Name", "Unknown");
  name_fr = atrebas_get_string (properties, "FrenchName", name);
  nld_id = json_object_get_string_member (feature, "id");
  slug = atrebas_get_string (properties, "Slug", "");
  uri = atrebas_get_string (properties, "description", "https://native-land.ca");
  uri_fr = atrebas_get_string (properties, "FrenchDescription", uri);
  theme = json_object_get_int_member_with_default (properties,
                                                   "theme",
                                                   ATREBAS_MAP_THEME_TERRITORY);

  geometry = json_object_get_object_member (feature, "geometry");
  coordinates = json_object_get_array_member (geometry, "coordinates");

  return g_object_new (ATREBAS_TYPE_FEATURE,
                       "color",       color,
                       "coordinates", coordinates,
                       "name",        name,
                       "name-fr",     name_fr,
                       "nld-id",      nld_id,
                       "slug",        slug,
                       "theme",       theme,
                       "uri",         uri,
                       "uri-fr",      uri_fr,
                       NULL);
}

