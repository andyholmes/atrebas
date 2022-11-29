// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2015 Lars Uebernickel
// SPDX-FileCopyrightText: 2015 Ryan Lortie
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-search-model"

#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>

#include "atrebas-backend.h"
#include "atrebas-feature.h"
#include "atrebas-macros.h"
#include "atrebas-search-model.h"


/**
 * SECTION:atrebassearchresults
 * @short_description: A list model for search results
 * @name: AtrebasSearchModel
 * @stability: Unstable
 * @include: atrebas.h
 *
 * The #AtrebasSearchModel class is an implementation of #GListModel for
 * #AtrebasBackend search results.
 */

struct _AtrebasSearchModel
{
  GObject         parent_instance;

  GeocodeBackend *backend;
  GCancellable   *cancellable;
  char           *query;
  double          latitude;
  double          longitude;
  unsigned int    update_id;

  /* GListModel */
  GSequence      *items;
  unsigned int    last_position;
  GSequenceIter  *last_iter;
  gboolean        last_position_valid;
};

/* Interfaces */
static void g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (AtrebasSearchModel, atrebas_search_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init));

enum {
  PROP_0,
  PROP_BACKEND,
  PROP_LATITUDE,
  PROP_LONGITUDE,
  PROP_QUERY,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * AtrebasBackend Callbacks
 */
static void
forward_cb (GeocodeBackend *backend,
            GAsyncResult   *result,
            gpointer        user_data)
{
  g_autoptr (AtrebasSearchModel) self = user_data;
  g_autolist (GeocodePlace) ret = NULL;
  g_autoptr (GError) error = NULL;
  GSequenceIter *it;
  unsigned int removed;
  unsigned int added;

  g_assert (ATREBAS_IS_SEARCH_MODEL (self));

  ret = geocode_backend_forward_search_finish (backend, result, &error);

  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      if (!g_error_matches (error, GEOCODE_ERROR, GEOCODE_ERROR_NO_MATCHES))
        {
          g_warning ("Search: %s", error->message);
          return;
        }
    }

  /* Remove old results */
  removed = g_sequence_get_length (self->items);
  g_sequence_remove_range (g_sequence_get_begin_iter (self->items),
                           g_sequence_get_end_iter (self->items));

  /* Add new results */
  added = g_list_length (ret);
  it = g_sequence_get_begin_iter (self->items);

  for (const GList *iter = ret; iter; iter = iter->next)
    g_sequence_insert_before (it, g_object_ref (iter->data));

  /* Notify */
  self->last_position = 0;
  self->last_position_valid = FALSE;
  g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);
}

static void
reverse_cb (GeocodeBackend *backend,
            GAsyncResult   *result,
            gpointer        user_data)
{
  g_autoptr (AtrebasSearchModel) self = user_data;
  g_autolist (GeocodePlace) ret = NULL;
  g_autoptr (GError) error = NULL;
  GSequenceIter *it;
  unsigned int removed;
  unsigned int added;

  g_assert (ATREBAS_IS_SEARCH_MODEL (self));

  ret = geocode_backend_reverse_resolve_finish (backend, result, &error);

  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      if (!g_error_matches (error, GEOCODE_ERROR, GEOCODE_ERROR_NO_MATCHES))
        {
          g_warning ("Search: %s", error->message);
          return;
        }
    }

  /* Remove old results */
  removed = g_sequence_get_length (self->items);
  g_sequence_remove_range (g_sequence_get_begin_iter (self->items),
                           g_sequence_get_end_iter (self->items));

  /* Add new results */
  added = g_list_length (ret);
  it = g_sequence_get_begin_iter (self->items);

  for (const GList *iter = ret; iter; iter = iter->next)
    g_sequence_insert_before (it, g_object_ref (iter->data));

  /* Notify */
  self->last_position = 0;
  self->last_position_valid = FALSE;
  g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);
}


/*
 * GListModel
 */
static GType
atrebas_search_model_get_item_type (GListModel *model)
{
  g_assert (ATREBAS_IS_SEARCH_MODEL (model));

  return GEOCODE_TYPE_PLACE;
}

static unsigned int
atrebas_search_model_get_n_items (GListModel *model)
{
  AtrebasSearchModel *self = ATREBAS_SEARCH_MODEL (model);

  g_assert (ATREBAS_IS_SEARCH_MODEL (self));

  return g_sequence_get_length (self->items);
}

static gpointer
atrebas_search_model_get_item (GListModel   *model,
                           unsigned int  position)
{
  AtrebasSearchModel *self = ATREBAS_SEARCH_MODEL (model);
  GSequenceIter *it = NULL;

  if (self->last_position_valid)
    {
      if (position < G_MAXUINT && self->last_position == position + 1)
        it = g_sequence_iter_prev (self->last_iter);
      else if (position > 0 && self->last_position == position - 1)
        it = g_sequence_iter_next (self->last_iter);
      else if (self->last_position == position)
        it = self->last_iter;
    }

  if (it == NULL)
    it = g_sequence_get_iter_at_pos (self->items, position);

  self->last_iter = it;
  self->last_position = position;
  self->last_position_valid = TRUE;

  if (g_sequence_iter_is_end (it))
    return NULL;

  return g_object_ref (g_sequence_get (it));
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = atrebas_search_model_get_item_type;
  iface->get_n_items = atrebas_search_model_get_n_items;
  iface->get_item = atrebas_search_model_get_item;
}


/*
 * AtrebasSearchModel
 */
static gboolean
atrebas_search_model_update_idle (gpointer data)
{
  AtrebasSearchModel *self = ATREBAS_SEARCH_MODEL (data);

  g_assert (ATREBAS_IS_SEARCH_MODEL (self));

  /* Cancel any ongoing request */
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->update_id = 0;

  /* If we have a position, perform a reverse search */
  if (self->latitude != 0.0 || self->longitude != 0.0)
    {
      g_autoptr (GHashTable) params = NULL;

      self->cancellable = g_cancellable_new ();
      params = atrebas_geocode_parameters_for_coordinates (self->latitude,
                                                       self->longitude);
      geocode_backend_reverse_resolve_async (self->backend,
                                             params,
                                             self->cancellable,
                                             (GAsyncReadyCallback)reverse_cb,
                                             g_object_ref (self));

    }

  /* If we have a query string, perform a forward search */
  else if (!atrebas_str_empty0 (self->query))
    {
      g_autoptr (GHashTable) params = NULL;

      self->cancellable = g_cancellable_new ();
      params = atrebas_geocode_parameters_for_location (self->query);
      geocode_backend_forward_search_async (self->backend,
                                            params,
                                            self->cancellable,
                                            (GAsyncReadyCallback)forward_cb,
                                            g_object_ref (self));
    }

  /* Otherwise just clear the results */
  else
    {
      unsigned int removed;

      removed = g_sequence_get_length (self->items);
      g_sequence_remove_range (g_sequence_get_begin_iter (self->items),
                               g_sequence_get_end_iter (self->items));

      self->last_position = 0;
      self->last_position_valid = FALSE;
      g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, 0);
    }

  return G_SOURCE_REMOVE;
}

static void
atrebas_search_model_update (AtrebasSearchModel *self)
{
  if (self->update_id == 0)
    {
      self->update_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                         atrebas_search_model_update_idle,
                                         g_object_ref (self),
                                         g_object_unref);
    }
}


/*
 * GObject
 */
static void
atrebas_search_model_constructed (GObject *object)
{
  AtrebasSearchModel *self = ATREBAS_SEARCH_MODEL (object);

  if (self->backend == NULL)
    self->backend = g_object_ref (atrebas_backend_get_default ());

  G_OBJECT_CLASS (atrebas_search_model_parent_class)->constructed (object);
}

static void
atrebas_search_model_dispose (GObject *object)
{
  AtrebasSearchModel *self = ATREBAS_SEARCH_MODEL (object);

  g_clear_handle_id (&self->update_id, g_source_remove);
  g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (atrebas_search_model_parent_class)->dispose (object);
}

static void
atrebas_search_model_finalize (GObject *object)
{
  AtrebasSearchModel *self = ATREBAS_SEARCH_MODEL (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->backend);
  g_clear_pointer (&self->query, g_free);
  g_clear_pointer (&self->items, g_sequence_free);

  G_OBJECT_CLASS (atrebas_search_model_parent_class)->finalize (object);
}

static void
atrebas_search_model_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  AtrebasSearchModel *self = ATREBAS_SEARCH_MODEL (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      g_value_set_object (value, self->backend);
      break;

    case PROP_LATITUDE:
      g_value_set_double (value, self->latitude);
      break;

    case PROP_LONGITUDE:
      g_value_set_double (value, self->longitude);
      break;

    case PROP_QUERY:
      g_value_set_string (value, self->query);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_search_model_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  AtrebasSearchModel *self = ATREBAS_SEARCH_MODEL (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      self->backend = g_value_dup_object (value);
      break;

    case PROP_LATITUDE:
      atrebas_search_model_set_latitude (self, g_value_get_double (value));
      break;

    case PROP_LONGITUDE:
      atrebas_search_model_set_longitude (self, g_value_get_double (value));
      break;

    case PROP_QUERY:
      atrebas_search_model_set_query (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
atrebas_search_model_class_init (AtrebasSearchModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = atrebas_search_model_constructed;
  object_class->dispose = atrebas_search_model_dispose;
  object_class->finalize = atrebas_search_model_finalize;
  object_class->get_property = atrebas_search_model_get_property;
  object_class->set_property = atrebas_search_model_set_property;

  /**
   * AtrebasSearchModel:backend:
   *
   * The #AtrebasBackend provided #GeocodePlace objects for this model.
   */
  properties [PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "Backend",
                         "The backend",
                         ATREBAS_TYPE_BACKEND,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * AtrebasSearchModel:latitude:
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
   * AtrebasSearchModel:longitude:
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
   * AtrebasSearchModel:query:
   *
   * The search query the results are for.
   */
  properties [PROP_QUERY] =
    g_param_spec_string ("query",
                         "Query",
                         "The search query",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
atrebas_search_model_init (AtrebasSearchModel *self)
{
  self->items = g_sequence_new (g_object_unref);
  self->last_position = 0;
  self->last_position_valid = FALSE;
}

/**
 * atrebas_search_model_new:
 * @backend: (nullable): a #GeocodeBackend
 *
 * Get a #GListModel of search results.
 *
 * If @backend is not %NULL it will be used as a source for #GeocodePlace
 * objects, otherwise the result of atrebas_backend_get_default() will be used.
 *
 * Returns: (transfer full): a #AtrebasSearchModel
 */
GListModel *
atrebas_search_model_new (GeocodeBackend *backend)
{
  return g_object_new (ATREBAS_TYPE_SEARCH_MODEL,
                       "backend", backend,
                       NULL);
}

/**
 * atrebas_search_model_get_backend:
 * @results: a #AtrebasSearchModel
 *
 * Get the #AtrebasBackend providing #GeocodePlace objects for @results.
 *
 * Returns: (transfer none): a #GeocodeBackend
 */
GeocodeBackend *
atrebas_search_model_get_backend (AtrebasSearchModel *search)
{
  g_return_val_if_fail (ATREBAS_IS_SEARCH_MODEL (search), NULL);

  return search->backend;
}

/**
 * atrebas_search_model_get_latitude:
 * @search: a #AtrebasSearchModel
 *
 * Get the north-south position of @search.
 *
 * Returns: a latitude
 */
double
atrebas_search_model_get_latitude (AtrebasSearchModel *search)
{
  g_return_val_if_fail (ATREBAS_IS_SEARCH_MODEL (search), 0.0);

  return search->latitude;
}

/**
 * atrebas_search_model_set_latitude:
 * @search: a #AtrebasSearchModel
 * @latitude: a north-south position
 *
 * Set the north-south position of @search to @latitude.
 */
void
atrebas_search_model_set_latitude (AtrebasSearchModel *search,
                               double          latitude)
{
  g_return_if_fail (ATREBAS_IS_SEARCH_MODEL (search));
  g_return_if_fail (latitude >= -90.0 && latitude <= 90.0);

  if (search->latitude == latitude)
    return;

  g_clear_pointer (&search->query, g_free);

  search->latitude = latitude;
  g_object_notify_by_pspec (G_OBJECT (search), properties [PROP_LATITUDE]);

  atrebas_search_model_update (search);
}

/**
 * atrebas_search_model_get_longitude:
 * @search: a #AtrebasSearchModel
 *
 * Get the east-west position of @search.
 *
 * Returns: a longitude
 */
double
atrebas_search_model_get_longitude (AtrebasSearchModel *search)
{
  g_return_val_if_fail (ATREBAS_IS_SEARCH_MODEL (search), 0.0);

  return search->longitude;
}

/**
 * atrebas_search_model_set_longitude:
 * @search: a #AtrebasSearchModel
 * @longitude: an east-west position
 *
 * Set the east-west of @search to @longitude.
 */
void
atrebas_search_model_set_longitude (AtrebasSearchModel *search,
                                double          longitude)
{
  g_return_if_fail (ATREBAS_IS_SEARCH_MODEL (search));
  g_return_if_fail (longitude >= -180.0 && longitude <= 180.0);

  if (search->longitude == longitude)
    return;

  g_clear_pointer (&search->query, g_free);

  search->longitude = longitude;
  g_object_notify_by_pspec (G_OBJECT (search), properties [PROP_LONGITUDE]);

  atrebas_search_model_update (search);
}

/**
 * atrebas_search_model_get_query:
 * @results: a #AtrebasSearchModel
 *
 * Get the search query that produced @results.
 *
 * Returns: (transfer none) (nullable): a query string
 */
const char *
atrebas_search_model_get_query (AtrebasSearchModel *search)
{
  g_return_val_if_fail (ATREBAS_IS_SEARCH_MODEL (search), NULL);

  return search->query;
}

/**
 * atrebas_search_model_set_query:
 * @results: a #AtrebasSearchModel
 *
 * Set the search query that produces @results.
 */
void
atrebas_search_model_set_query (AtrebasSearchModel *search,
                            const char     *query)
{
  g_return_if_fail (ATREBAS_IS_SEARCH_MODEL (search));

  if (g_strcmp0 (search->query, query) == 0)
    return;

  search->latitude = 0.0;
  search->longitude = 0.0;
  g_clear_pointer (&search->query, g_free);

  search->query = g_strdup (query);
  g_object_notify_by_pspec (G_OBJECT (search), properties [PROP_QUERY]);

  atrebas_search_model_update (search);
}

