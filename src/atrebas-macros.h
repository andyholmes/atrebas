// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define atrebas_str_empty0(str)       (!(str) || !*(str))
#define atrebas_str_equal(str1,str2)  (strcmp(str1,str2)==0)
#define atrebas_str_equal0(str1,str2) (g_strcmp0(str1,str2)==0)


/**
 * ATREBAS_MIN_LATITUDE: (value -85.0511287798)
 *
 * The minimal possible latitude value.
 */
#define ATREBAS_MIN_LATITUDE  (-85.0511287798)

/**
 * ATREBAS_MAX_LATITUDE: (value 85.0511287798)
 *
 * The maximal possible latitude value.
 */
#define ATREBAS_MAX_LATITUDE  (85.0511287798)

/**
 * ATREBAS_MIN_LONGITUDE: (value -180.0)
 *
 * The minimal possible longitude value.
 */
#define ATREBAS_MIN_LONGITUDE (-180.0)

/**
 * ATREBAS_MAX_LONGITUDE: (value 180.0)
 *
 * The maximal possible longitude value.
 */
#define ATREBAS_MAX_LONGITUDE (180.0)

/**
 * ATREBAS_IS_LATITUDE:
 * @latitude: a north-west position
 *
 * Checks that @latitude is in the valid range for Web Mercator projections.
 * This can be used in g_return_if_fail() checks.
 */
#define ATREBAS_IS_LATITUDE(latitude) (latitude >= ATREBAS_MIN_LATITUDE && latitude <= ATREBAS_MAX_LATITUDE)

/**
 * ATREBAS_IS_LONGITUDE:
 * @longitude: an east-west position
 *
 * Checks that @longitude is in the valid range for Web Mercator projections.
 * This can be used in g_return_if_fail() checks.
 */
#define ATREBAS_IS_LONGITUDE(longitude) ((longitude >= ATREBAS_MIN_LONGITUDE) && (longitude <= ATREBAS_MAX_LONGITUDE))


static inline gint64
(atrebas_get_int) (JsonObject *object,
               const char *member,
               gint64      fallback)
{
  JsonNode *node;

  if G_UNLIKELY ((node = json_object_get_member (object, member)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_INT64)
    return fallback;

  return json_node_get_int (node);
}
#define atrebas_get_int(o,m,f) (atrebas_get_int(o,m,f))


static inline const char *
(atrebas_get_string) (JsonObject *object,
                  const char *member,
                  const char *fallback)
{
  JsonNode *node;

  if G_UNLIKELY ((node = json_object_get_member (object, member)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    return fallback;

  return json_node_get_string (node);
}
#define atrebas_get_string(o,m,f) (atrebas_get_string(o,m,f))

G_END_DECLS

