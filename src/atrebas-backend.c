// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>
// SPDX-FileCopyrightText: Copyright 1994-2006 W Randolph Franklin (WRF)

#define G_LOG_DOMAIN "atrebas-backend"

#include "config.h"

#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <sqlite3.h>

#include "atrebas-backend.h"
#include "atrebas-backend-private.h"
#include "atrebas-feature.h"
#include "atrebas-macros.h"


#define NATIVE_LAND_API     "https://native-land.ca/api/index.php"
#define QUERY_DEFAULT_LIMIT 1000


/**
 * SECTION:atrebasbackend
 * @short_description: A geocode source for indigenous territories
 * @title: AtrebasBackend
 * @stability: Unstable
 * @include: atrebas.h
 *
 * The #AtrebasBackend class is a #GeocodeBackend implementation for the
 * <native-land.ca> API, providing indigenous language, territory and treaty
 * themed maps.
 */

struct _AtrebasBackend
{
  GObject       parent_instance;

  SoupSession  *session;
  sqlite3      *connection;
  char         *path;
  sqlite3_stmt *stmts[5];
  GAsyncQueue  *operations;
  unsigned int  closed : 1;
};

/* Interfaces */
static void   geocode_backend_iface_init (GeocodeBackendInterface *iface);

G_DEFINE_TYPE_WITH_CODE (AtrebasBackend, atrebas_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GEOCODE_TYPE_BACKEND, geocode_backend_iface_init))

enum {
  PROP_0,
  PROP_PATH,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  STMT_ADD_FEATURE,
  STMT_GET_FEATURE,
  STMT_GET_FEATURES,
  STMT_REMOVE_FEATURE,
  STMT_SEARCH_FEATURES,
  N_STATEMENTS,
};

static gconstpointer statements[N_STATEMENTS] = { NULL, };

static GeocodeBackend *default_backend = NULL;


/*
 * The source URIs for native-land.ca
 */
typedef struct
{
  char        *uri;
  AtrebasMapTheme  theme;
} MapSource;

static inline void
map_source_free (gpointer data)
{
  MapSource *source = data;

  g_clear_pointer (&source->uri, g_free);
  g_clear_pointer (&source, g_free);
}

/* Native Lands Digital */
static MapSource nl_languages = {
  "https://native-land.ca/api/index.php?maps=languages",
  ATREBAS_MAP_THEME_LANGUAGE,
};

static MapSource nl_territories = {
  "https://native-land.ca/api/index.php?maps=territories",
  ATREBAS_MAP_THEME_TERRITORY,
};

static MapSource nl_treaties = {
  "https://native-land.ca/api/index.php?maps=treaties",
  ATREBAS_MAP_THEME_TREATY,
};

static MapSource *remote_sources[] = {
  &nl_languages,
  &nl_territories,
  &nl_treaties,
};

/* Bundled GeoJSON */
static MapSource bundled_languages = {
  "file://"PACKAGE_DATADIR"/maps/indigenousLanguages.json",
  ATREBAS_MAP_THEME_LANGUAGE,
};

static MapSource bundled_territories = {
  "file://"PACKAGE_DATADIR"/maps/indigenousTerritories.json",
  ATREBAS_MAP_THEME_TERRITORY,
};

static MapSource bundled_treaties = {
  "file://"PACKAGE_DATADIR"/maps/indigenousTreaties.json",
  ATREBAS_MAP_THEME_TREATY,
};

static MapSource *local_sources[] = {
  &bundled_languages,
  &bundled_territories,
  &bundled_treaties,
};


/**
 * geojson_pnpoly:
 * @coordinates: a #JsonArray
 * @x: X-axis coordinate of the test point
 * @y: Y-axis coordinate of the test point
 *
 * Check if the point (@x, @y) is within the boundaries of the polygon described
 * by @coordinates.
 *
 * @coordinates must be the `coordinates` field of a GeoJSON fragment containing
 * a `Polygon` type.
 *
 * Based on "pnpoly":
 *     Copyright 1994-2006 W Randolph Franklin (WRF)
 *     https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html
 *
 * Returns: %TRUE if inside, %FALSE if outside
 */
static inline gboolean
geojson_pnpoly (JsonArray *coordinates,
                double     x,
                double     y)
{
  gboolean ret = FALSE;
  JsonArray *polygon;
  unsigned int n_vertices = 0;

  polygon = json_array_get_array_element (coordinates, 0);
  n_vertices = json_array_get_length (polygon);

  for (unsigned int i = 0, j = n_vertices - 1; i < n_vertices; j = i++)
    {
      JsonArray *next = json_array_get_array_element (polygon, i);
      double nextx = json_array_get_double_element (next, 0);
      double nexty = json_array_get_double_element (next, 1);

      JsonArray *prev = json_array_get_array_element (polygon, j);
      double prevx = json_array_get_double_element (prev, 0);
      double prevy = json_array_get_double_element (prev, 1);

      if ((nexty > y) != (prevy > y) &&
          (x < (prevx - nextx) * (y - nexty) / (prevy - nexty) + nextx))
       ret = !ret;
    }

  return ret;
}

/**
 * geojson_intersect:
 * @coordinates: a #JsonArray
 * @left: left extent
 * @top: top extent
 * @right: right extent
 * @bottom: bottom extent
 *
 * Check if the bounding box defined by @top, @right, @bottom and @left
 * intersects with the extents of the polygon described by @coordinates.
 *
 * @coordinates must be the `coordinates` field of a GeoJSON fragment containing
 * a `Polygon` type.
 *
 * Returns: %TRUE if inside, %FALSE if outside
 */
static inline gboolean
geojson_intersect (JsonArray *coordinates,
                   double     left,
                   double     top,
                   double     right,
                   double     bottom)
{
  JsonArray *polygon;
  unsigned int n_vertices = 0;
  double max_x = 0.0;
  double min_x = 0.0;
  double max_y = 0.0;
  double min_y = 0.0;

  polygon = json_array_get_array_element (coordinates, 0);
  n_vertices = json_array_get_length (polygon);

  for (unsigned int i = 0; i < n_vertices; i++)
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

  return (min_x <= right && left <= max_x &&
          min_y <= bottom && top <= max_y);
}


// https://gist.github.com/sahib/2622023
// Utf8 aware levenshtein
static inline int
levenshtein_strcmp0 (const char *name1,
                     const char *name2)
{
  g_autofree char *s = NULL;
  g_autofree char *t = NULL;
  int n, m;

  if G_UNLIKELY (!g_utf8_validate (name1, -1, NULL) ||
                 !g_utf8_validate (name2, -1, NULL))
    return 0;

  s = g_utf8_normalize (name1, -1, G_NORMALIZE_ALL_COMPOSE);
  n = (s) ? g_utf8_strlen (s, -1) + 1 : 0;

  t = g_utf8_normalize (name2, -1, G_NORMALIZE_ALL_COMPOSE);
  m = (t) ? g_utf8_strlen (t, -1) + 1 : 0;

  // Nothing to compute really..
  if G_UNLIKELY (n < 2)
      return m;

  if G_UNLIKELY (m < 2)
    return n;

  // String matrix
  int d[n][m];

  // Init first row|column to 0...n|m
  for (int i = 0; i < n; i++)
    d[i][0] = i;

  for (int j = 0; j < m; j++)
    d[0][j] = j;

  for (int i = 1; i < n; i++)
    {
      // Current char in string s
      gunichar cats = g_utf8_get_char (g_utf8_offset_to_pointer (s, i - 1));

      for (int j = 1; j < m; j++)
        {
          // Do -1 only once
          int jm1 = j - 1;
          int im1 = i - 1;

          gunichar tats = g_utf8_get_char (g_utf8_offset_to_pointer (t, jm1));

          // a = above cell, b = left cell, c = left above celli
          int a = d[im1][j] + 1;
          int b = d[i][jm1] + 1;
          int c = d[im1][jm1] + (tats != cats);

          // Compute the minimum of a,b,c and set MIN(a,b,c) to cell d[i][j]
          d[i][j] = (a < b) ? MIN (a, c) : MIN (b, c);
        }
    }

  // The result is stored in the very right down cell
  return d[n - 1][m - 1];
}

static int
levenshtein_sort (gconstpointer a,
                  gconstpointer b,
                  gpointer      user_data)
{
  const char *name1 = geocode_place_get_name (GEOCODE_PLACE (a));
  const char *name2 = geocode_place_get_name (GEOCODE_PLACE (b));
  const char *query = user_data;

  return levenshtein_strcmp0 (query, name1) - levenshtein_strcmp0 (query, name2);
}


/*
 * Operation Queue
 */
typedef enum
{
  OPERATION_DEFAULT,
  OPERATION_CRITICAL,
  OPERATION_TERMINAL,
} OperationMode;

typedef struct
{
  GTask           *task;
  GTaskThreadFunc  task_func;
  OperationMode    task_mode;
} OperationClosure;

static inline void
operation_closure_free (gpointer data)
{
  OperationClosure *closure = data;

  g_clear_object (&closure->task);
  g_clear_pointer (&closure, g_free);
}

static inline void
operation_closure_cancel (gpointer data)
{
  OperationClosure *closure = data;

  if (G_IS_TASK (closure->task) && !g_task_get_completed (closure->task))
    {
      g_task_return_new_error (closure->task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "Operation cancelled");
    }

  g_clear_pointer (&closure, operation_closure_free);
}

static gpointer
atrebas_backend_thread (gpointer data)
{
  g_autoptr (GAsyncQueue) tasks = data;
  OperationClosure *closure = NULL;

  while ((closure = g_async_queue_pop (tasks)) != NULL)
    {
      unsigned int mode = closure->task_mode;

      if (G_IS_TASK (closure->task) && !g_task_get_completed (closure->task))
        {
          closure->task_func (closure->task,
                              g_task_get_source_object (closure->task),
                              g_task_get_task_data (closure->task),
                              g_task_get_cancellable (closure->task));

          if (mode == OPERATION_CRITICAL && g_task_had_error (closure->task))
            mode = OPERATION_TERMINAL;
        }

      g_clear_pointer (&closure, operation_closure_free);

      if (mode == OPERATION_TERMINAL)
        break;
    }

  /* Cancel any queued tasks */
  g_async_queue_lock (tasks);

  while ((closure = g_async_queue_try_pop_unlocked (tasks)) != NULL)
    g_clear_pointer (&closure, operation_closure_cancel);

  g_async_queue_unlock (tasks);

  return NULL;
}

static void
atrebas_backend_thread_push (AtrebasBackend      *self,
                         GTask           *task,
                         GTaskThreadFunc  task_func,
                         OperationMode    task_mode)
{
  OperationClosure *closure;

  g_assert (ATREBAS_IS_BACKEND (self));
  g_assert (G_IS_TASK (task));
  g_assert (task_func != NULL);

  closure = g_new0 (OperationClosure, 1);
  closure->task = g_object_ref (task);
  closure->task_func = task_func;
  closure->task_mode = task_mode;

  g_async_queue_lock (self->operations);

  if (!self->closed)
    {
      self->closed = (closure->task_mode == OPERATION_TERMINAL);
      g_async_queue_push_unlocked (self->operations, g_steal_pointer (&closure));
    }

  g_async_queue_unlock (self->operations);

  g_clear_pointer (&closure, operation_closure_cancel);
}

/*
 * BackendQuery
 */
typedef struct
{
  char         *location;
  double        latitude;
  double        longitude;
  unsigned int  limit;
  unsigned int  bounded : 1;
  struct
    {
      double left;
      double top;
      double right;
      double bottom;
    } viewbox;
} BackendQuery;

static BackendQuery *
backend_query_new (GHashTable *parameters)
{
  BackendQuery *query = NULL;
  GValue *value;

  query = g_new0 (BackendQuery, 1);
  query->limit = QUERY_DEFAULT_LIMIT;

  if (g_hash_table_contains (parameters, "lat") &&
      g_hash_table_contains (parameters, "lon"))
    {
      value = g_hash_table_lookup (parameters, "lat");
      query->latitude = g_value_get_double (value);

      value = g_hash_table_lookup (parameters, "lon");
      query->longitude = g_value_get_double (value);
    }

  /* A custom Geocode parameter; a free-form search string */
  if ((value = g_hash_table_lookup (parameters, "location")) != NULL &&
      G_VALUE_HOLDS_STRING (value))
    query->location = g_value_dup_string (value);

  /* A custom Geocode parameter; defines a limit on results. */
  if ((value = g_hash_table_lookup (parameters, "limit")) != NULL &&
      G_VALUE_HOLDS_UINT (value))
    query->limit = g_value_get_uint (value);

  /* A custom Geocode parameter; defines whether the search results are
   * restricted to a specific area.
   *
   * TODO: https://gitlab.gnome.org/GNOME/geocode-glib/-/issues/25
   */
  if ((value = g_hash_table_lookup (parameters, "bounded")) != NULL &&
      G_VALUE_HOLDS_STRING (value))
    {
      query->bounded = g_str_equal (g_value_get_string (value), "1");

      if ((value = g_hash_table_lookup (parameters, "viewbox")) != NULL &&
          G_VALUE_HOLDS_STRING (value))
        {
          const char *viewbox;
          g_auto (GStrv) bounds = NULL;

          viewbox = g_value_get_string (value);
          bounds = g_strsplit (viewbox, ",", -1);

          if (g_strv_length (bounds) == 4)
            {
              query->viewbox.left = g_ascii_strtod (bounds[0], NULL);
              query->viewbox.top = g_ascii_strtod (bounds[1], NULL);
              query->viewbox.right = g_ascii_strtod (bounds[2], NULL);
              query->viewbox.bottom = g_ascii_strtod (bounds[3], NULL);
            }
        }
    }

  return query;
}

static void
backend_query_free (gpointer data)
{
  BackendQuery *query = data;

  g_clear_pointer (&query->location, g_free);
  g_free (query);
}


/*
 * Step functions
 */
static inline AtrebasFeature *
atrebas_backend_get_feature_step (sqlite3_stmt  *stmt,
                              GError       **error)
{
  int rc;
  g_autoptr (JsonNode) coordinates_node = NULL;
  const char *coordinates;

  g_assert (stmt != NULL);
  g_assert (error == NULL || *error == NULL);

  if ((rc = sqlite3_step (stmt)) == SQLITE_DONE)
    return NULL;

  if (rc != SQLITE_ROW)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s: %s", G_STRFUNC, sqlite3_errstr (rc));
      return NULL;
    }

  coordinates = (const char *)sqlite3_column_text (stmt, 6);
  coordinates_node = json_from_string (coordinates, NULL);

  return g_object_new (ATREBAS_TYPE_FEATURE,
                       "nld-id",      sqlite3_column_text (stmt, 0),
                       "name",        sqlite3_column_text (stmt, 1),
                       "name_fr",     sqlite3_column_text (stmt, 2),
                       "uri",         sqlite3_column_text (stmt, 3),
                       "uri_fr",      sqlite3_column_text (stmt, 4),
                       "color",       sqlite3_column_text (stmt, 5),
                       "coordinates", json_node_get_array (coordinates_node),
                       "slug",        sqlite3_column_text (stmt, 7),
                       "theme",       sqlite3_column_int (stmt, 8),
                       NULL);
}

static inline gboolean
atrebas_backend_set_feature_step (sqlite3_stmt  *stmt,
                              JsonObject    *feature,
                              GError       **error)
{
  int rc;
  JsonObject *props;
  JsonObject *geometry;
  JsonNode *coordinates_node;
  const char *id;
  const char *name;
  const char *name_fr;
  const char *uri;
  const char *uri_fr;
  const char *color;
  const char *slug;
  AtrebasMapTheme theme;
  g_autofree char *coordinates = NULL;

  /* Extract the map data */
  id = json_object_get_string_member (feature, "id");

  props = json_object_get_object_member (feature, "properties");
  name = json_object_get_string_member_with_default (props,
                                                     "Name",
                                                     "Unknown");
  name_fr = json_object_get_string_member_with_default (props,
                                                        "FrenchName",
                                                        name);
  uri = json_object_get_string_member_with_default (props,
                                                    "description",
                                                    "https://native-land.ca");
  uri_fr = json_object_get_string_member_with_default (props,
                                                       "FrenchDescription",
                                                       uri);
  color = json_object_get_string_member_with_default (props,
                                                      "color",
                                                      "#000000");
  slug = json_object_get_string_member_with_default (props,
                                                     "Slug",
                                                     "");
  theme = json_object_get_int_member_with_default (props,
                                                   "theme",
                                                   ATREBAS_MAP_THEME_TERRITORY);

  geometry = json_object_get_object_member (feature, "geometry");
  coordinates_node = json_object_get_member (geometry, "coordinates");
  coordinates = json_to_string (coordinates_node, FALSE);

  /* Bind the message data */
  sqlite3_bind_text (stmt, 1, id, -1, NULL);
  sqlite3_bind_text (stmt, 2, name, -1, NULL);
  sqlite3_bind_text (stmt, 3, name_fr, -1, NULL);
  sqlite3_bind_text (stmt, 4, uri, -1, NULL);
  sqlite3_bind_text (stmt, 5, uri_fr, -1, NULL);
  sqlite3_bind_text (stmt, 6, color, -1, NULL);
  sqlite3_bind_text (stmt, 7, coordinates, -1, NULL);
  sqlite3_bind_text (stmt, 8, slug, -1, NULL);
  sqlite3_bind_int (stmt, 9, theme);

  /* Execute and auto-reset */
  if ((rc = sqlite3_step (stmt)) != SQLITE_DONE)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s: %s", G_STRFUNC, sqlite3_errstr (rc));
      sqlite3_reset (stmt);
      return FALSE;
    }

  sqlite3_reset (stmt);
  return TRUE;
}

static inline gboolean
atrebas_backend_bounded_feature_step (sqlite3_stmt  *stmt,
                                  BackendQuery  *query,
                                  AtrebasFeature   **feature,
                                  GError       **error)
{
  int rc;
  const char *coordinates_text;
  g_autoptr (JsonNode) coordinates_node = NULL;
  JsonArray *coordinates_list = NULL;

  g_assert (stmt != NULL);
  g_assert (error == NULL || *error == NULL);

  if ((rc = sqlite3_step (stmt)) == SQLITE_DONE)
    return FALSE;

  if (rc != SQLITE_ROW)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s: %s", G_STRFUNC, sqlite3_errstr (rc));
      return FALSE;
    }

  coordinates_text = (const char *)sqlite3_column_text (stmt, 6);
  coordinates_node = json_from_string (coordinates_text, NULL);
  coordinates_list = json_node_get_array (coordinates_node);

  if (!geojson_intersect (coordinates_list,
                          query->viewbox.left,
                          query->viewbox.top,
                          query->viewbox.right,
                          query->viewbox.bottom))
    {
      *feature = NULL;
      return TRUE;
    }

  *feature = g_object_new (ATREBAS_TYPE_FEATURE,
                           "nld-id",      sqlite3_column_text (stmt, 0),
                           "name",        sqlite3_column_text (stmt, 1),
                           "name_fr",     sqlite3_column_text (stmt, 2),
                           "uri",         sqlite3_column_text (stmt, 3),
                           "uri_fr",      sqlite3_column_text (stmt, 4),
                           "color",       sqlite3_column_text (stmt, 5),
                           "coordinates", json_node_get_array (coordinates_node),
                           "slug",        sqlite3_column_text (stmt, 7),
                           "theme",       sqlite3_column_int (stmt, 8),
                           NULL);

  return TRUE;
}

static inline gboolean
atrebas_backend_locate_feature_step (sqlite3_stmt  *stmt,
                                 BackendQuery  *query,
                                 AtrebasFeature   **feature,
                                 GError       **error)
{
  int rc;
  const char *coordinates_text;
  g_autoptr (JsonNode) coordinates_node = NULL;
  JsonArray *coordinates_list = NULL;

  g_assert (stmt != NULL);
  g_assert (error == NULL || *error == NULL);

  if ((rc = sqlite3_step (stmt)) == SQLITE_DONE)
    return FALSE;

  if (rc != SQLITE_ROW)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s: %s", G_STRFUNC, sqlite3_errstr (rc));
      return FALSE;
    }

  coordinates_text = (const char *)sqlite3_column_text (stmt, 6);
  coordinates_node = json_from_string (coordinates_text, NULL);
  coordinates_list = json_node_get_array (coordinates_node);

  if (!geojson_pnpoly (coordinates_list, query->longitude, query->latitude))
    {
      *feature = NULL;
      return TRUE;
    }

  *feature = g_object_new (ATREBAS_TYPE_FEATURE,
                           "nld-id",      sqlite3_column_text (stmt, 0),
                           "name",        sqlite3_column_text (stmt, 1),
                           "name_fr",     sqlite3_column_text (stmt, 2),
                           "uri",         sqlite3_column_text (stmt, 3),
                           "uri_fr",      sqlite3_column_text (stmt, 4),
                           "color",       sqlite3_column_text (stmt, 5),
                           "coordinates", json_node_get_array (coordinates_node),
                           "slug",        sqlite3_column_text (stmt, 7),
                           "theme",       sqlite3_column_int (stmt, 8),
                           NULL);

  return TRUE;
}


/*
 * Database GTaskFuncs
 */
static void
atrebas_backend_lookup_task (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  AtrebasBackend *self = ATREBAS_BACKEND (source_object);
  const char *id = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_GET_FEATURE];
  g_autoptr (AtrebasFeature) ret = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  /* Collect the results */
  sqlite3_bind_text (stmt, 1, id, -1, NULL);
  ret = atrebas_backend_get_feature_step (stmt, &error);
  sqlite3_reset (stmt);

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, g_steal_pointer (&ret), g_object_unref);
}


/*
 * GeocodeBackend GTaskFuncs
 */
static inline void
_place_list_free (gpointer data)
{
  g_list_free_full ((GList *)data, g_object_unref);
}

static void
atrebas_backend_forward_search_task (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  AtrebasBackend *self = ATREBAS_BACKEND (source_object);
  BackendQuery *query = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_SEARCH_FEATURES];
  g_autolist (AtrebasFeature) ret = NULL;
  g_autofree char *query_param = NULL;
  AtrebasFeature *feature = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  // NOTE: escaped percent signs (%%) are query wildcards (%)
  query_param = g_strdup_printf ("%%%s%%", query->location);
  sqlite3_bind_text (stmt, 1, query_param, -1, NULL);
  sqlite3_bind_int (stmt, 2, query->limit);

  /* Collect the results */
  if (query->bounded)
    {
      while (atrebas_backend_bounded_feature_step (stmt, query, &feature, &error))
        {
          if (feature != NULL)
            ret = g_list_prepend (ret, feature);
        }
      sqlite3_reset (stmt);
    }
  else
    {
      while ((feature = atrebas_backend_get_feature_step (stmt, &error)) != NULL)
        ret = g_list_prepend (ret, feature);
      sqlite3_reset (stmt);
    }

  if (error != NULL)
    return g_task_return_error (task, error);

  if (ret == NULL)
    {
      g_task_return_new_error (task,
                               GEOCODE_ERROR,
                               GEOCODE_ERROR_NO_MATCHES,
                               "No matches found for request");
      return;
    }

  ret = g_list_sort_with_data (ret, levenshtein_sort, (char *)query);
  g_task_return_pointer (task, g_steal_pointer (&ret), _place_list_free);
}

static void
atrebas_backend_reverse_resolve_task (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  AtrebasBackend *self = ATREBAS_BACKEND (source_object);
  BackendQuery *query = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_GET_FEATURES];
  g_autolist (AtrebasFeature) ret = NULL;
  AtrebasFeature *feature = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  /* Collect the results */
  while (atrebas_backend_locate_feature_step (stmt, query, &feature, &error))
    {
      if (feature != NULL)
        ret = g_list_prepend (ret, feature);
    }
  sqlite3_reset (stmt);

  if (error != NULL)
    return g_task_return_error (task, error);

  if (ret == NULL)
    {
      g_task_return_new_error (task,
                               GEOCODE_ERROR,
                               GEOCODE_ERROR_NO_MATCHES,
                               "No matches found for request");
      return;
    }

  ret = g_list_reverse (ret);
  g_task_return_pointer (task, g_steal_pointer (&ret), _place_list_free);
}


/*
 * Database Update GTaskFuncs
 */
static gboolean
atrebas_backend_load_features (AtrebasBackend   *self,
                           JsonNode     *node,
                           AtrebasMapTheme   theme,
                           GError      **error)
{
  sqlite3_stmt *stmt = self->stmts[STMT_ADD_FEATURE];
  JsonArray *features;
  unsigned int n_features = 0;

  g_assert (ATREBAS_IS_BACKEND (self));
  g_assert (error == NULL || *error == NULL);


  if (node == NULL || json_node_get_value_type (node) != JSON_TYPE_ARRAY)
    {
      g_set_error_literal (error,
                           GEOCODE_ERROR,
                           GEOCODE_ERROR_PARSE,
                           "Unsupported GeoJSON fragment");
      return FALSE;
    }

  features = json_node_get_array (node);
  n_features = json_array_get_length (features);

  for (unsigned int i = 0; i < n_features; i++)
    {
      JsonObject *feature = json_array_get_object_element (features, i);
      JsonObject *props;
      g_autoptr (GError) warn = NULL;

      props = json_object_get_object_member (feature, "properties");
      json_object_set_int_member (props, "theme", theme);

      if (!atrebas_backend_set_feature_step (stmt, feature, &warn))
        g_warning ("Parsing feature: %s", warn->message);
    }

  return TRUE;
}

static gboolean
atrebas_backend_load_geojson (AtrebasBackend   *self,
                          JsonNode     *collection,
                          AtrebasMapTheme   theme,
                          GError      **error)
{
  JsonObject *object;
  JsonNode *node;

  g_assert (ATREBAS_IS_BACKEND (self));
  g_assert (JSON_NODE_HOLDS_OBJECT (collection));
  g_assert (error == NULL || *error == NULL);

  object = json_node_get_object (collection);

  if ((node = json_object_get_member (object, "type")) == NULL ||
      json_node_get_value_type (node) != G_TYPE_STRING ||
      g_strcmp0 (json_node_get_string (node), "FeatureCollection") != 0)
    {
      g_set_error_literal (error,
                           GEOCODE_ERROR,
                           GEOCODE_ERROR_PARSE,
                           "Unsupported GeoJSON file");
      return FALSE;
    }

  if ((node = json_object_get_member (object, "features")) == NULL ||
      json_node_get_value_type (node) != JSON_TYPE_ARRAY)
    {
      g_set_error_literal (error,
                           GEOCODE_ERROR,
                           GEOCODE_ERROR_PARSE,
                           "Unsupported GeoJSON file");
      return FALSE;
    }

  return atrebas_backend_load_features (self, node, theme, error);
}

static void
atrebas_backend_update_task (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  AtrebasBackend *self = ATREBAS_BACKEND (source_object);
  g_autoptr (JsonParser) parser = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  parser = json_parser_new ();

  for (unsigned int i = 0; i < G_N_ELEMENTS (remote_sources); i++)
    {
      MapSource *source = remote_sources[i];
      g_autoptr (SoupMessage) message = NULL;
      g_autoptr (GInputStream) response = NULL;
      g_autoptr (JsonNode) root = NULL;

      if (g_task_return_error_if_cancelled (task))
        return;

      message = soup_message_new ("GET", source->uri);
      response = soup_session_send (self->session, message, cancellable, &error);

      if (error != NULL)
        return g_task_return_error (task, error);

      if (!json_parser_load_from_stream (parser, response, cancellable, &error))
        return g_task_return_error (task, error);

      root = json_parser_steal_root (parser);

      if (!atrebas_backend_load_features (self, root, source->theme, &error))
        return g_task_return_error (task, error);
    }

  g_task_return_boolean (task, TRUE);
}

static void
atrebas_backend_update_local_task (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  AtrebasBackend *self = ATREBAS_BACKEND (source_object);
  g_autoptr (JsonParser) parser = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  parser = json_parser_new ();

  for (unsigned int i = 0; i < G_N_ELEMENTS (local_sources); i++)
    {
      MapSource *source = local_sources[i];
      g_autoptr (GFile) file = NULL;
      g_autoptr (GFileInputStream) stream = NULL;
      g_autoptr (JsonNode) root = NULL;

      if (g_task_return_error_if_cancelled (task))
        return;

      file = g_file_new_for_uri (source->uri);

      if ((stream = g_file_read (file, cancellable, &error)) == NULL)
        return g_task_return_error (task, error);
      
      if (!json_parser_load_from_stream (parser, G_INPUT_STREAM (stream), cancellable, &error))
        return g_task_return_error (task, error);

      root = json_parser_steal_root (parser);

      if (!atrebas_backend_load_geojson (self, root, source->theme, &error))
        return g_task_return_error (task, error);
    }

  g_task_return_boolean (task, TRUE);
}

static void
atrebas_backend_load_task (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  AtrebasBackend *self = ATREBAS_BACKEND (source_object);
  MapSource *source = task_data;
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (JsonNode) root = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  parser = json_parser_new ();

  if (!json_parser_load_from_file (parser, source->uri, &error))
    return g_task_return_error (task, error);

  root = json_parser_steal_root (parser);

  if (!atrebas_backend_load_geojson (self, root, source->theme, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}


/*
 * Core Database GTaskFuncs
 */
static void
atrebas_backend_open_task (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  AtrebasBackend *self = ATREBAS_BACKEND (source_object);
  int rc;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (self->connection != NULL)
    return g_task_return_boolean (task, TRUE);

  /* If the database hasn't been created, update from bundled JSON */
  if (!g_file_test (self->path, G_FILE_TEST_IS_REGULAR))
    {
      g_autoptr (GTask) db_task = NULL;

      db_task = g_task_new (self, cancellable, NULL, NULL);
      atrebas_backend_thread_push (self,
                                   db_task,
                                   atrebas_backend_update_local_task,
                                   OPERATION_DEFAULT);
    }

  /* Pass NOMUTEX since tasks are executed sequentially */
  rc = sqlite3_open_v2 (self->path,
                        &self->connection,
                        (SQLITE_OPEN_READWRITE |
                         SQLITE_OPEN_CREATE |
                         SQLITE_OPEN_NOMUTEX),
                        NULL);

  if (rc != SQLITE_OK)
    {
      g_task_return_new_error (task,
                               GEOCODE_ERROR,
                               GEOCODE_ERROR_INTERNAL_SERVER,
                               "sqlite3_open_v2(): \"%s\": [%i] %s",
                               self->path, rc, sqlite3_errstr (rc));
      g_clear_pointer (&self->connection, sqlite3_close);
      return;
    }

  /* Prepare the tables */
  rc = sqlite3_exec (self->connection,
                     ATREBAS_BACKEND_FEATURE_TABLE_SQL,
                     NULL,
                     NULL,
                     NULL);

  if (rc != SQLITE_OK)
    {
      g_task_return_new_error (task,
                               GEOCODE_ERROR,
                               GEOCODE_ERROR_INTERNAL_SERVER,
                               "sqlite3_prepare_v2(): [%i] %s",
                               rc, sqlite3_errstr (rc));
      g_clear_pointer (&self->connection, sqlite3_close);
      return;
    }

  /* Prepare the statements */
  for (unsigned int i = 0; i < N_STATEMENTS; i++)
    {
      const char *sql = statements[i];
      sqlite3_stmt *stmt = NULL;

      rc = sqlite3_prepare_v2 (self->connection, sql, -1, &stmt, NULL);

      if (rc != SQLITE_OK)
        {
          g_task_return_new_error (task,
                                   GEOCODE_ERROR,
                                   GEOCODE_ERROR_INTERNAL_SERVER,
                                   "sqlite3_prepare_v2(): \"%s\": [%i] %s",
                                   sql, rc, sqlite3_errstr (rc));
          g_clear_pointer (&self->connection, sqlite3_close);
          return;
        }

      self->stmts[i] = g_steal_pointer (&stmt);
    }

  g_task_return_boolean (task, TRUE);
}

static void
atrebas_backend_close_task (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  AtrebasBackend *self = ATREBAS_BACKEND (source_object);
  int rc;

  if (g_task_return_error_if_cancelled (task))
    return;

  g_assert (self->connection != NULL);

  /* Cleanup cached statements */
  for (unsigned int i = 0; i < N_STATEMENTS; i++)
    g_clear_pointer (&self->stmts[i], sqlite3_finalize);

  /* Optimize the database before closing.
   *
   * See:
   *   https://www.sqlite.org/pragma.html#pragma_optimize
   *   https://www.sqlite.org/queryplanner-ng.html#update_2017_a_better_fix
   */
  rc = sqlite3_exec (self->connection, "PRAGMA optimize;", NULL, NULL, NULL);

  if (rc != SQLITE_OK)
    {
      g_debug ("sqlite3_exec(): \"%s\": [%i] %s",
               "PRAGMA optimize;", rc, sqlite3_errstr (rc));
    }

  /* Close the connection */
  if ((rc = sqlite3_close (self->connection)) != SQLITE_OK)
    {
      g_task_return_new_error (task,
                               GEOCODE_ERROR,
                               GEOCODE_ERROR_INTERNAL_SERVER,
                               "sqlite3_close(): [%i] %s",
                               rc, sqlite3_errstr (rc));
      return;
    }

  self->connection = NULL;
  g_task_return_boolean (task, TRUE);
}

/*
 * AtrebasBackend
 */
static void
atrebas_backend_operation_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!g_task_propagate_boolean (G_TASK (result), &error))
    g_warning ("%s: %s", G_STRFUNC, error->message);
}

static void
atrebas_backend_open (AtrebasBackend *self)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GThread) thread = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (ATREBAS_IS_BACKEND (self));

  task = g_task_new (self, NULL, atrebas_backend_operation_cb, NULL);
  g_task_set_source_tag (task, atrebas_backend_open);

  atrebas_backend_thread_push (self,
                           task,
                           atrebas_backend_open_task,
                           OPERATION_CRITICAL);

  thread = g_thread_try_new ("atrebas-backend",
                             atrebas_backend_thread,
                             g_async_queue_ref (self->operations),
                             &error);

  if G_UNLIKELY (error != NULL)
    {
      g_critical ("%s: %s", G_STRFUNC, error->message);
      self->closed = TRUE;
    }
}

static void
atrebas_backend_close (AtrebasBackend *self)
{
  g_autoptr (GTask) task = NULL;

  g_assert (ATREBAS_IS_BACKEND (self));

  if (self->closed)
    return;

  task = g_task_new (self, NULL, atrebas_backend_operation_cb, NULL);
  g_task_set_source_tag (task, atrebas_backend_close);

  atrebas_backend_thread_push (self,
                           task,
                           atrebas_backend_close_task,
                           OPERATION_TERMINAL);

  while (!g_task_get_completed (task))
    g_main_context_iteration (NULL, FALSE);

  self->closed = TRUE;
}


/*
 * GeocodeBackend
 */
static GList *
atrebas_backend_forward_search (GeocodeBackend  *backend,
                            GHashTable      *params,
                            GCancellable    *cancellable,
                            GError         **error)
{
  AtrebasBackend *self = ATREBAS_BACKEND (backend);
  g_autoptr (GTask) task = NULL;
  BackendQuery *query = NULL;

  g_assert (ATREBAS_IS_BACKEND (self));

  if (!g_hash_table_contains (params, "location"))
    {
      g_set_error (error,
                   GEOCODE_ERROR,
                   GEOCODE_ERROR_INVALID_ARGUMENTS,
                   "Missing location parameter");
      return NULL;
    }

  query = backend_query_new (params);
  task = g_task_new (backend, cancellable, NULL, NULL);
  g_task_set_source_tag (task, atrebas_backend_forward_search);
  g_task_set_task_data (task, g_steal_pointer (&query), backend_query_free);
  atrebas_backend_thread_push (self,
                           task,
                           atrebas_backend_forward_search_task,
                           OPERATION_DEFAULT);

  /* Iterate the main context until the task completes */
  while (!g_task_get_completed (task))
    g_main_context_iteration (NULL, FALSE);

  return g_task_propagate_pointer (task, error);
}

static void
atrebas_backend_forward_search_async (GeocodeBackend      *backend,
                                  GHashTable          *params,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  AtrebasBackend *self = ATREBAS_BACKEND (backend);
  g_autoptr (GTask) task = NULL;
  BackendQuery *query = NULL;

  g_assert (ATREBAS_IS_BACKEND (self));

  if (!g_hash_table_contains (params, "location"))
    {
      g_task_report_new_error (backend, callback, user_data,
                               atrebas_backend_forward_search_async,
                               GEOCODE_ERROR,
                               GEOCODE_ERROR_INVALID_ARGUMENTS,
                               "Missing location parameter");
      return;
    }

  query = backend_query_new (params);
  task = g_task_new (backend, cancellable, callback, user_data);
  g_task_set_source_tag (task, atrebas_backend_forward_search_async);
  g_task_set_task_data (task, g_steal_pointer (&query), backend_query_free);
  atrebas_backend_thread_push (self,
                           task,
                           atrebas_backend_forward_search_task,
                           OPERATION_DEFAULT);
}

static GList *
atrebas_backend_reverse_resolve (GeocodeBackend  *backend,
                             GHashTable      *params,
                             GCancellable    *cancellable,
                             GError         **error)
{
  AtrebasBackend *self = ATREBAS_BACKEND (backend);
  g_autoptr (GTask) task = NULL;
  BackendQuery *query = NULL;

  g_assert (ATREBAS_IS_BACKEND (self));

  if (!g_hash_table_contains (params, "lat") ||
      !g_hash_table_contains (params, "lon"))
    {
      g_set_error (error,
                   GEOCODE_ERROR,
                   GEOCODE_ERROR_INVALID_ARGUMENTS,
                   "Missing `lat` and `lon` parameters");
      return NULL;
    }

  query = backend_query_new (params);
  task = g_task_new (backend, cancellable, NULL, NULL);
  g_task_set_source_tag (task, atrebas_backend_reverse_resolve);
  g_task_set_task_data (task, g_steal_pointer (&query), backend_query_free);
  atrebas_backend_thread_push (self,
                           task,
                           atrebas_backend_reverse_resolve_task,
                           OPERATION_DEFAULT);

  /* Iterate the main context until the task completes */
  while (!g_task_get_completed (task))
    g_main_context_iteration (NULL, FALSE);

  return g_task_propagate_pointer (task, error);
}

static void
atrebas_backend_reverse_resolve_async (GeocodeBackend      *backend,
                                   GHashTable          *params,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  AtrebasBackend *self = ATREBAS_BACKEND (backend);
  g_autoptr (GTask) task = NULL;
  BackendQuery *query = NULL;

  g_assert (ATREBAS_IS_BACKEND (self));

  if (!g_hash_table_contains (params, "lat") ||
      !g_hash_table_contains (params, "lon"))
    {
      g_task_report_new_error (backend, callback, user_data,
                               atrebas_backend_reverse_resolve_async,
                               GEOCODE_ERROR,
                               GEOCODE_ERROR_INVALID_ARGUMENTS,
                               "Missing `lat` and `lon` parameters");
    }

  query = backend_query_new (params);
  task = g_task_new (backend, cancellable, callback, user_data);
  g_task_set_source_tag (task, atrebas_backend_reverse_resolve_async);
  g_task_set_task_data (task, g_steal_pointer (&query), backend_query_free);
  atrebas_backend_thread_push (self,
                           task,
                           atrebas_backend_reverse_resolve_task,
                           OPERATION_DEFAULT);
}

static void
geocode_backend_iface_init (GeocodeBackendInterface *iface)
{
  iface->forward_search = atrebas_backend_forward_search;
  iface->forward_search_async = atrebas_backend_forward_search_async;
  iface->reverse_resolve = atrebas_backend_reverse_resolve;
  iface->reverse_resolve_async = atrebas_backend_reverse_resolve_async;
}


/*
 * GObject
 */
static void
atrebas_backend_constructed (GObject *object)
{
  AtrebasBackend *self = ATREBAS_BACKEND (object);
  g_autofree char *dirname = NULL;

  /* Ensure we have a database path in an existing */
  if (self->path == NULL)
    self->path = g_build_filename (g_get_user_cache_dir (),
                                   PACKAGE_NAME,
                                   "cache.db",
                                   NULL);

  dirname = g_path_get_dirname (self->path);

  if (g_mkdir_with_parents (dirname, 0700) == -1)
    g_critical ("Creating '%s': %s", dirname, g_strerror (errno));

  atrebas_backend_open (self);

  G_OBJECT_CLASS (atrebas_backend_parent_class)->constructed (object);
}

static void
atrebas_backend_dispose (GObject *object)
{
  AtrebasBackend *self = ATREBAS_BACKEND (object);

  atrebas_backend_close (self);

  G_OBJECT_CLASS (atrebas_backend_parent_class)->dispose (object);
}

static void
atrebas_backend_finalize (GObject *object)
{
  AtrebasBackend *self = ATREBAS_BACKEND (object);

  atrebas_backend_close (self);

  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->operations, g_async_queue_unref);
  g_clear_object (&self->session);

  G_OBJECT_CLASS (atrebas_backend_parent_class)->finalize (object);
}

static void
atrebas_backend_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  AtrebasBackend *self = ATREBAS_BACKEND (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, atrebas_backend_get_path (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_backend_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  AtrebasBackend *self = ATREBAS_BACKEND (object);

  switch (prop_id)
    {
    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_backend_class_init (AtrebasBackendClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = atrebas_backend_constructed;
  object_class->dispose = atrebas_backend_dispose;
  object_class->finalize = atrebas_backend_finalize;
  object_class->get_property = atrebas_backend_get_property;
  object_class->set_property = atrebas_backend_set_property;

  /**
   * AtrebasBackend:path:
   *
   * Path to the sqlite3 database for this #AtrebasBackend.
   */
  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Path",
                         "Path to the database",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /*
   * SQL Statements
   */
  statements[STMT_ADD_FEATURE] = ADD_FEATURE_SQL;
  statements[STMT_GET_FEATURE] = GET_FEATURE_SQL;
  statements[STMT_GET_FEATURES] = GET_FEATURES_SQL;
  statements[STMT_REMOVE_FEATURE] = REMOVE_FEATURE_SQL;
  statements[STMT_SEARCH_FEATURES] = SEARCH_FEATURES_SQL;
}

static void
atrebas_backend_init (AtrebasBackend *self)
{
  self->operations = g_async_queue_new_full (operation_closure_cancel);
  self->session = soup_session_new ();
}

/**
 * atrebas_backend_get_default:
 *
 * Get the default #AtrebasBackend.
 *
 * Returns: (transfer none): The default backend
 */
GeocodeBackend *
atrebas_backend_get_default (void)
{
  if (default_backend == NULL)
    {
      default_backend = g_object_new (ATREBAS_TYPE_BACKEND, NULL);
      g_object_add_weak_pointer (G_OBJECT (default_backend),
                                 (gpointer)&default_backend);
    }

  return default_backend;
}

/**
 * atrebas_backend_new:
 * @path: (type filename) (nullable): path to the database
 *
 * Create a new #AtrebasBackend.
 *
 * You most likely want to call atrebas_backend_get_default() instead of this
 * function.
 *
 * Returns: (transfer full): a new #AtrebasBackend.
 */
GeocodeBackend *
atrebas_backend_new (const char *path)
{
  return g_object_new (ATREBAS_TYPE_BACKEND,
                       "path", path,
                       NULL);
}

/**
 * atrebas_backend_get_path:
 * @backend: a #AtrebasBackend
 *
 * Get the path to the sqlite3 database for @backend.
 *
 * Returns: (type filename) (transfer none): a filepath
 */
const char *
atrebas_backend_get_path (AtrebasBackend *backend)
{
  g_return_val_if_fail (ATREBAS_IS_BACKEND (backend), NULL);

  return backend->path;
}

/**
 * atrebas_backend_update:
 * @backend: a #AtrebasBackend
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Update the backend from <native-land.ca>. Call
 * atrebas_backend_update_finish() to get the result.
 */
void
atrebas_backend_update (AtrebasBackend      *backend,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (ATREBAS_IS_BACKEND (backend));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (backend, cancellable, callback, user_data);
  g_task_set_source_tag (task, atrebas_backend_update);
  atrebas_backend_thread_push (backend,
                           task,
                           atrebas_backend_update_task,
                           OPERATION_DEFAULT);
}

/**
 * atrebas_backend_update_finish:
 * @backend: a #AtrebasBackend
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by atrebas_backend_update().
 *
 * Returns: %TRUE or %FALSE with @error set
 */
gboolean
atrebas_backend_update_finish (AtrebasBackend  *backend,
                               GAsyncResult    *result,
                               GError         **error)
{
  g_return_val_if_fail (ATREBAS_IS_BACKEND (backend), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, backend), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * atrebas_backend_load:
 * @backend: a #AtrebasBackend
 * @filename: (type filename): a file path
 * @theme: a #AtrebasMapTheme
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Load the GeoJSON feature collection at @filename.
 */
void
atrebas_backend_load (AtrebasBackend      *backend,
                      const char          *filename,
                      AtrebasMapTheme      theme,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  MapSource *source;

  g_return_if_fail (ATREBAS_IS_BACKEND (backend));
  g_return_if_fail (!atrebas_str_empty0 (filename));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  source = g_new0 (MapSource, 1);
  source->uri = g_strdup (filename);
  source->theme = theme;

  task = g_task_new (backend, cancellable, callback, user_data);
  g_task_set_source_tag (task, atrebas_backend_load);
  g_task_set_task_data (task, source, map_source_free);
  atrebas_backend_thread_push (backend,
                           task,
                           atrebas_backend_load_task,
                           OPERATION_DEFAULT);
}

/**
 * atrebas_backend_load_finish:
 * @backend: a #AtrebasBackend
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by atrebas_backend_load().
 *
 * Returns: %TRUE or %FALSE with @error set
 */
gboolean
atrebas_backend_load_finish (AtrebasBackend  *backend,
                             GAsyncResult    *result,
                             GError         **error)
{
  g_return_val_if_fail (ATREBAS_IS_BACKEND (backend), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, backend), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * atrebas_backend_load:
 * @backend: a #AtrebasBackend
 * @id: a feature ID
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Lookup a #AtrebasFeature for @id. Call atrebas_backend_lookup_finish() to get the
 * result.
 */
void
atrebas_backend_lookup (AtrebasBackend      *backend,
                        const char          *id,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (ATREBAS_IS_BACKEND (backend));
  g_return_if_fail (id != NULL && *id != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (backend, cancellable, callback, user_data);
  g_task_set_source_tag (task, atrebas_backend_load);
  g_task_set_task_data (task, g_strdup (id), g_free);
  atrebas_backend_thread_push (backend,
                           task,
                           atrebas_backend_lookup_task,
                           OPERATION_DEFAULT);
}

/**
 * atrebas_backend_lookup_finish:
 * @backend: a #AtrebasBackend
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by atrebas_backend_lookup().
 *
 * Returns: (transfer full) (nullable): a #AtrebasFeature
 */
AtrebasFeature *
atrebas_backend_lookup_finish (AtrebasBackend    *backend,
                           GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (ATREBAS_IS_BACKEND (backend), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, backend), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}

