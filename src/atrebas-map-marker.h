// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <shumate/shumate.h>

#include "atrebas-feature.h"

G_BEGIN_DECLS

#define ATREBAS_TYPE_MAP_MARKER (atrebas_map_marker_get_type())

G_DECLARE_FINAL_TYPE (AtrebasMapMarker, atrebas_map_marker, ATREBAS, MAP_MARKER, ShumateMarker)

GtkWidget    * atrebas_map_marker_new           (GeocodePlace *place);
GListModel   * atrebas_map_marker_get_features  (AtrebasMapMarker *marker);
GeocodePlace * atrebas_map_marker_get_place     (AtrebasMapMarker *marker);
void           atrebas_map_marker_set_place     (AtrebasMapMarker *marker,
                                             GeocodePlace *place);
const char   * atrebas_map_marker_get_icon_name (AtrebasMapMarker *marker);
void           atrebas_map_marker_set_icon_name (AtrebasMapMarker *marker,
                                             const char   *name);
int            atrebas_map_marker_get_icon_size (AtrebasMapMarker *marker);
void           atrebas_map_marker_set_icon_size (AtrebasMapMarker *marker,
                                             int           size);

G_END_DECLS
