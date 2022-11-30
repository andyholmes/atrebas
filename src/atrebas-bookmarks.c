// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-bookmarks"

#include "config.h"

#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>

#include "atrebas-backend.h"
#include "atrebas-bookmarks.h"
#include "atrebas-feature.h"


/**
 * SECTION:atrebasbookmarks
 * @short_description: A bookmark manager
 * @title: AtrebasBookmarks
 * @stability: Unstable
 *
 * #AtrebasBookmarks is a simple bookmark manager that stores data in #GSettings. It
 * implements the #GListModel interface for convenience.
 */

struct _AtrebasBookmarks
{
  GObject        parent_instance;

  GSettings     *settings;

  /* GListModel */
  GSequence     *items;
  unsigned int   last_position;
  GSequenceIter *last_iter;
  gboolean       last_position_valid;
};

static void  atrebas_bookmarks_add_place_internal (AtrebasBookmarks        *self,
                                               GeocodePlace        *place);

/* Interfaces */
static void  g_list_model_iface_init          (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (AtrebasBookmarks, atrebas_bookmarks, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init));

static GListModel *default_bookmarks = NULL;


/*
 * GListModel
 */
static GType
atrebas_bookmarks_get_item_type (GListModel *model)
{
  g_assert (ATREBAS_IS_BOOKMARKS (model));

  return GEOCODE_TYPE_PLACE;
}

static unsigned int
atrebas_bookmarks_get_n_items (GListModel *model)
{
  AtrebasBookmarks *self = ATREBAS_BOOKMARKS (model);

  g_assert (ATREBAS_IS_BOOKMARKS (self));

  return g_sequence_get_length (self->items);
}

static gpointer
atrebas_bookmarks_get_item (GListModel   *model,
                        unsigned int  position)
{
  AtrebasBookmarks *self = ATREBAS_BOOKMARKS (model);
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
  iface->get_item_type = atrebas_bookmarks_get_item_type;
  iface->get_n_items = atrebas_bookmarks_get_n_items;
  iface->get_item = atrebas_bookmarks_get_item;
}


/*
 * AtrebasBookmarks
 */
static void
load_feature_cb (AtrebasBackend   *backend,
                 GAsyncResult *result,
                 AtrebasBookmarks *self)
{
  g_autoptr (AtrebasFeature) feature = NULL;
  g_autoptr (GError) error = NULL;

  if ((feature = atrebas_backend_lookup_finish (backend, result, &error)) != NULL)
    atrebas_bookmarks_add_place_internal (self, GEOCODE_PLACE (feature));

  if (error != NULL)
    g_debug ("%s: %s", G_STRFUNC, error->message);

  g_object_unref (self);
}

static void
load_place_cb (GeocodeReverse *reverse,
               GAsyncResult   *result,
               AtrebasBookmarks   *self)
{
  g_autoptr (GeocodePlace) place = NULL;
  g_autoptr (GError) error = NULL;

  if ((place = geocode_reverse_resolve_finish (reverse, result, &error)) != NULL)
    atrebas_bookmarks_add_place_internal (self, place);

  if (error != NULL)
    g_debug ("%s: %s", G_STRFUNC, error->message);

  g_object_unref (self);
}

static inline GVariant *
atrebas_bookmarks_serialize_place (GeocodePlace *place)
{
  GVariantBuilder builder;
  GeocodeLocation *location;
  const char *name;
  const char *id;
  double latitude = 0.0;
  double longitude = 0.0;
  double accuracy = GEOCODE_LOCATION_ACCURACY_UNKNOWN;

  g_assert (GEOCODE_IS_PLACE (place));

  name = geocode_place_get_name (place);
  location = geocode_place_get_location (place);
  latitude = geocode_location_get_latitude (location);
  longitude = geocode_location_get_longitude (location);
  accuracy = geocode_location_get_accuracy (location);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add_parsed (&builder, "{'name', <%s>}", name);
  g_variant_builder_add_parsed (&builder, "{'latitude', <%d>}", latitude);
  g_variant_builder_add_parsed (&builder, "{'longitude', <%d>}", longitude);
  g_variant_builder_add_parsed (&builder, "{'accuracy', <%d>}", accuracy);

  if (ATREBAS_IS_FEATURE (place))
    {
      AtrebasFeature *feature = ATREBAS_FEATURE (place);

      g_variant_builder_add_parsed (&builder, "{'nld-id', <%s>}",
                                    atrebas_feature_get_nld_id (feature));
    }
  else if ((id = geocode_place_get_osm_id (place)) != NULL)
    {
      g_variant_builder_add_parsed (&builder, "{'osm-id', <%s>}", id);
    }

  return g_variant_builder_end (&builder);
}

static inline int
atrebas_bookmarks_equal_func (gconstpointer a,
                          gconstpointer b,
                          gpointer      user_data)
{
  GeocodePlace *place1 = (GeocodePlace *)a;
  GeocodePlace *place2 = (GeocodePlace *)b;
  const char *id1 = NULL;
  const char *id2 = NULL;

  if (ATREBAS_IS_FEATURE (place1) && ATREBAS_IS_FEATURE (place2) &&
      atrebas_feature_equal ((AtrebasFeature *)place1, (AtrebasFeature *)place2))
    return 0;

  if ((id1 = geocode_place_get_osm_id (place1)) != NULL &&
      (id2 = geocode_place_get_osm_id (place2)) != NULL &&
      g_strcmp0 (id1, id2) == 0)
    return 0;

  return g_utf8_collate (geocode_place_get_name (place1),
                         geocode_place_get_name (place2));
}

static void
atrebas_bookmarks_add_place_internal (AtrebasBookmarks *self,
                                  GeocodePlace *place)
{
  GSequenceIter *it;
  unsigned int position;

  g_assert (ATREBAS_IS_BOOKMARKS (self));
  g_assert (GEOCODE_IS_PLACE (place));

  it = g_sequence_insert_sorted (self->items,
                                 g_object_ref (place),
                                 atrebas_bookmarks_equal_func,
                                 self);
  position = g_sequence_iter_get_position (it);

  if (position <= self->last_position)
    {
      self->last_iter = NULL;
      self->last_position = 0;
      self->last_position_valid = FALSE;
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

static void
atrebas_bookmarks_remove_place_internal (AtrebasBookmarks *self,
                                     GeocodePlace *place)
{
  GSequenceIter *iter;
  unsigned int position;

  g_assert (ATREBAS_IS_BOOKMARKS (self));
  g_assert (GEOCODE_IS_PLACE (place));

  iter = g_sequence_lookup (self->items,
                            place,
                            atrebas_bookmarks_equal_func,
                            self);

  if (iter == NULL)
    return;

  position = g_sequence_iter_get_position (iter);
  g_sequence_remove (iter);

  if (position <= self->last_position)
    {
      self->last_iter = NULL;
      self->last_position = 0;
      self->last_position_valid = FALSE;
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

static void
atrebas_bookmarks_load (AtrebasBookmarks *self)
{
  g_autoptr (GVariant) bookmarks = NULL;
  GVariantIter iter;
  GVariant *bookmark;

  g_assert (ATREBAS_IS_BOOKMARKS (self));

  bookmarks = g_settings_get_value (self->settings, "bookmarks");
  g_variant_iter_init (&iter, bookmarks);

  while (g_variant_iter_next (&iter, "@a{sv}", &bookmark))
    {
      const char *name = NULL;
      const char *osm_id = NULL;
      const char *nld_id = NULL;
      double latitude, longitude, accuracy;

      if (!g_variant_lookup (bookmark, "name", "&s", &name) ||
          !g_variant_lookup (bookmark, "latitude", "d", &latitude) ||
          !g_variant_lookup (bookmark, "longitude", "d", &longitude) ||
          !g_variant_lookup (bookmark, "accuracy", "d", &accuracy))
        goto next;

      /* A serialized #AtrebasFeature */
      if (g_variant_lookup (bookmark, "nld-id", "&s", &nld_id))
        {
          atrebas_backend_lookup (ATREBAS_BACKEND (atrebas_backend_get_default ()),
                              nld_id,
                              NULL,
                              (GAsyncReadyCallback)load_feature_cb,
                              g_object_ref (self));
        }

      /* A serialized #GeocodePlace */
      else if (g_variant_lookup (bookmark, "osm-id", "&s", &osm_id))
        {
          g_autoptr (GeocodeLocation) location = NULL;
          g_autoptr (GeocodeReverse) reverse = NULL;

          location = geocode_location_new (latitude, longitude, accuracy);
          reverse = geocode_reverse_new_for_location (location);
          geocode_reverse_resolve_async (reverse,
                                         NULL,
                                         (GAsyncReadyCallback)load_place_cb,
                                         g_object_ref (self));
        }

      next:
        g_clear_pointer (&bookmark, g_variant_unref);
    }
}

static void
atrebas_bookmarks_store (AtrebasBookmarks *self)
{
  GVariant *bookmarks = NULL;
  GVariantBuilder builder;
  GSequenceIter *iter;
  GSequenceIter *end;

  g_assert (ATREBAS_IS_BOOKMARKS (self));

  iter = g_sequence_get_begin_iter (self->items);
  end = g_sequence_get_end_iter (self->items);

  /* The last bookmark was removed */
  if (iter == end)
    {
      g_settings_reset (self->settings, "bookmarks");
      return;
    }

  /* Serialize the bookmarks for storage */
  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  while (iter != end)
    {
      GeocodePlace *place;
      GVariant *bookmark;

      place = g_sequence_get (iter);
      bookmark = atrebas_bookmarks_serialize_place (place);
      g_variant_builder_add_value (&builder, bookmark);

      iter = g_sequence_iter_next (iter);
    }

  bookmarks = g_variant_builder_end (&builder);

  /* Save the result */
  g_return_if_fail (g_variant_is_of_type (bookmarks, G_VARIANT_TYPE_ARRAY));
  g_settings_set_value (self->settings, "bookmarks", bookmarks);
}


/*
 * GObject
 */
static void
atrebas_bookmarks_finalize (GObject *object)
{
  AtrebasBookmarks *self = ATREBAS_BOOKMARKS (object);

  g_clear_pointer (&self->items, g_sequence_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (atrebas_bookmarks_parent_class)->finalize (object);
}

static void
atrebas_bookmarks_class_init (AtrebasBookmarksClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = atrebas_bookmarks_finalize;
}

static void
atrebas_bookmarks_init (AtrebasBookmarks *self)
{
  self->items = g_sequence_new (g_object_unref);
  self->last_position = 0;
  self->last_position_valid = FALSE;
  self->settings = g_settings_new ("ca.andyholmes.Atrebas");

  atrebas_bookmarks_load (self);
}

/**
 * atrebas_bookmarks_get_default:
 *
 * Get the default #AtrebasBookmarks.
 *
 * Returns: (transfer none): The default bookmarks
 */
GListModel *
atrebas_bookmarks_get_default (void)
{
  if (default_bookmarks == NULL)
    {
      default_bookmarks = g_object_new (ATREBAS_TYPE_BOOKMARKS, NULL);
      g_object_add_weak_pointer (G_OBJECT (default_bookmarks),
                                 (gpointer)&default_bookmarks);
    }

  return default_bookmarks;
}

/**
 * atrebas_bookmarks_add_place:
 * @bookmarks: (nullable): a #AtrebasBookmarks
 * @place: a #GeocodePlace
 *
 * Add @place to @bookmarks.
 *
 * If @bookmarks is %NULL, the default #AtrebasBookmarks will be used.
 */
void
atrebas_bookmarks_add_place (AtrebasBookmarks *bookmarks,
                         GeocodePlace *place)
{
  g_return_if_fail (bookmarks == NULL || ATREBAS_IS_BOOKMARKS (bookmarks));
  g_return_if_fail (GEOCODE_IS_PLACE (place));

  if (bookmarks == NULL)
    bookmarks = ATREBAS_BOOKMARKS (atrebas_bookmarks_get_default ());

  atrebas_bookmarks_add_place_internal (bookmarks, place);
  atrebas_bookmarks_store (bookmarks);
}

/**
 * atrebas_bookmarks_remove_place:
 * @bookmarks: (nullable): a #AtrebasBookmarks
 * @place: a #GeocodePlace
 *
 * Remove @place from @bookmarks.
 *
 * If @bookmarks is %NULL, the default #AtrebasBookmarks will be used.
 */
void
atrebas_bookmarks_remove_place (AtrebasBookmarks *bookmarks,
                            GeocodePlace *place)
{
  g_return_if_fail (bookmarks == NULL || ATREBAS_IS_BOOKMARKS (bookmarks));
  g_return_if_fail (GEOCODE_IS_PLACE (place));

  if (bookmarks == NULL)
    bookmarks = ATREBAS_BOOKMARKS (atrebas_bookmarks_get_default ());

  atrebas_bookmarks_remove_place_internal (bookmarks, place);
  atrebas_bookmarks_store (bookmarks);
}

/**
 * atrebas_bookmarks_has_place:
 * @bookmarks: (nullable): a #AtrebasBookmarks
 * @place: a #GeocodePlace
 *
 * Remove @place from @bookmarks.
 *
 * If @bookmarks is %NULL, the default #AtrebasBookmarks will be used.
 */
gboolean
atrebas_bookmarks_has_place (AtrebasBookmarks *bookmarks,
                         GeocodePlace *place)
{
  GSequenceIter *iter;

  g_return_val_if_fail (bookmarks == NULL || ATREBAS_IS_BOOKMARKS (bookmarks), FALSE);
  g_return_val_if_fail (GEOCODE_IS_PLACE (place), FALSE);

  if (bookmarks == NULL)
    bookmarks = ATREBAS_BOOKMARKS (atrebas_bookmarks_get_default ());

  iter = g_sequence_lookup (bookmarks->items,
                            place,
                            atrebas_bookmarks_equal_func,
                            bookmarks);

  return iter != NULL;
}

