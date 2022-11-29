// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-backend"

#include "config.h"

#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>
#include <math.h>

#include "atrebas-backend.h"


static inline GValue *
parameter_double (double value)
{
  GValue *ret;

  ret = g_new0 (GValue, 1);
  g_value_init (ret, G_TYPE_DOUBLE);
  g_value_set_double (ret, value);

  return ret;
}

static inline GValue *
parameter_string (const char *value)
{
  GValue *ret;

  ret = g_new0 (GValue, 1);
  g_value_init (ret, G_TYPE_STRING);
  g_value_set_string (ret, value);

  return ret;
}

static void
parameter_free (gpointer data)
{
  GValue *value = data;

  g_value_unset (value);
  g_free (value);
}

void
atrebas_geocode_parameters_add_double (GHashTable *parameters,
                                   const char *key,
                                   double      value)
{
  GValue *val;

  val = g_new0 (GValue, 1);
  g_value_init (val, G_TYPE_DOUBLE);
  g_value_set_double (val, value);

  g_hash_table_replace (parameters, (gpointer)key, val);
}

void
atrebas_geocode_parameters_add_string (GHashTable *parameters,
                                   const char *key,
                                   const char *value)
{
  GValue *val;

  val = g_new0 (GValue, 1);
  g_value_init (val, G_TYPE_STRING);
  g_value_set_string (val, value);

  g_hash_table_replace (parameters, (gpointer)key, val);
}

/**
 * atrebas_geocode_parameters_for_coordinates:
 * @latitude: a north-west position
 * @longitude: an east-west position
 *
 * Get a #GHashTable for @latitude and @longitude, prepared for geocode-glib.
 *
 * Returns: (transfer full) (element-type utf8 GLib.Value): a #GHashTable
 */
GHashTable *
atrebas_geocode_parameters_for_coordinates (double latitude,
                                        double longitude)
{
  GHashTable *ret;

  /* Semantics from http://xmpp.org/extensions/xep-0080.html */
  ret = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, parameter_free);
  g_hash_table_insert (ret, (gpointer)"lat", parameter_double (latitude));
  g_hash_table_insert (ret, (gpointer)"lon", parameter_double (longitude));

  return ret;
}

/**
 * atrebas_geocode_parameters_for_location:
 * @location: a free-form query string
 *
 * Get a #GHashTable for @location, prepared for geocode-glib.
 *
 * Returns: (transfer full) (element-type utf8 GLib.Value): a #GHashTable
 */
GHashTable *
atrebas_geocode_parameters_for_location (const char *location)
{
  GHashTable *ret;

  /* Semantics from http://xmpp.org/extensions/xep-0080.html */
  ret = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, parameter_free);
  g_hash_table_insert (ret, (gpointer)"location", parameter_string (location));

  return ret;
}

