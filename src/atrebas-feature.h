// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <geocode-glib/geocode-glib.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * AtrebasMapTheme:
 * @ATREBAS_FEATURE_LANGUAGE: Regional language
 * @ATREBAS_FEATURE_TERRITORY: Territory
 * @ATREBAS_FEATURE_TREATY: Treaty
 *
 * Enumeration of map themes provided by <native-land.ca>.
 */
typedef enum
{
  ATREBAS_MAP_THEME_LANGUAGE,
  ATREBAS_MAP_THEME_TERRITORY,
  ATREBAS_MAP_THEME_TREATY,
} AtrebasMapTheme;


/**
 * AtrebasPoint:
 * @latitude: a north-south position
 * @longitude: an east-west position
 *
 * #AtrebasPoint represents a latitude and longitude.
 */
typedef struct
{
  double latitude;
  double longitude;
} AtrebasPoint;


#define ATREBAS_TYPE_FEATURE (atrebas_feature_get_type())

G_DECLARE_FINAL_TYPE (AtrebasFeature, atrebas_feature, ATREBAS, FEATURE, GeocodePlace)

AtrebasFeature  * atrebas_feature_new             (const char      *name,
                                                   const char      *uri,
                                                   JsonArray       *coordinates);
const char      * atrebas_feature_get_name_fr     (AtrebasFeature  *feature);
const char      * atrebas_feature_get_nld_id      (AtrebasFeature  *feature);
const char      * atrebas_feature_get_uri         (AtrebasFeature  *feature);
const char      * atrebas_feature_get_uri_fr      (AtrebasFeature  *feature);
const char      * atrebas_feature_get_color       (AtrebasFeature  *feature);
JsonArray       * atrebas_feature_get_coordinates (AtrebasFeature  *feature);
const char      * atrebas_feature_get_slug        (AtrebasFeature  *feature);
AtrebasMapTheme   atrebas_feature_get_theme       (AtrebasFeature  *feature);

/* Convenience Functions */
gboolean          atrebas_feature_equal           (gconstpointer    feature1,
                                                   gconstpointer    feature2);
unsigned int      atrebas_feature_hash            (gconstpointer    feature);
gboolean          atrebas_feature_contains_point  (AtrebasFeature  *feature,
                                                   double           latitude,
                                                   double           longitude);

/* GeoJSON Support */
JsonNode        * atrebas_feature_serialize       (AtrebasFeature  *feature);
AtrebasFeature  * atrebas_feature_deserialize     (JsonNode        *node,
                                                   GError         **error);

G_END_DECLS

