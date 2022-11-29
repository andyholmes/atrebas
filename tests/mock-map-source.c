// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <shumate/shumate.h>

#include "mock-map-source.h"


G_DEFINE_TYPE (MockMapSource, mock_map_source, SHUMATE_TYPE_MAP_SOURCE);


/*
 * ShumateMapSource
 */
static void
mock_map_source_fill_tile_async (ShumateMapSource    *source,
                                 ShumateTile         *tile,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  MockMapSource *self = MOCK_MAP_SOURCE (source);
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (SHUMATE_IS_MAP_SOURCE (self));
  g_return_if_fail (SHUMATE_IS_TILE (tile));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}


/*
 * GObject
 */
static void
mock_map_source_class_init (MockMapSourceClass *klass)
{
  ShumateMapSourceClass *source_class = SHUMATE_MAP_SOURCE_CLASS (klass);

  source_class->fill_tile_async = mock_map_source_fill_tile_async;
}


static void
mock_map_source_init (MockMapSource *tile_source)
{
}

/**
 * mock_map_source_new:
 *
 * Create a new #MockMapSource.
 *
 * Returns: a new #MockMapSource
 */
ShumateMapSource *
mock_map_source_new (void)
{
  return g_object_new (MOCK_TYPE_MAP_SOURCE, NULL);
}

