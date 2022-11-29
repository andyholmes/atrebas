// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

#include "atrebas-feature.h"

G_BEGIN_DECLS

#define ATREBAS_TYPE_MAP_VIEW (atrebas_map_view_get_type())

G_DECLARE_FINAL_TYPE (AtrebasMapView, atrebas_map_view, ATREBAS, MAP_VIEW, GtkBox)

GtkWidget  * atrebas_map_view_new                  (void);
gboolean     atrebas_map_view_get_compact          (AtrebasMapView   *view);
void         atrebas_map_view_set_compact          (AtrebasMapView   *view,
                                                gboolean      compact);
double       atrebas_map_view_get_latitude         (AtrebasMapView   *view);
void         atrebas_map_view_set_latitude         (AtrebasMapView   *view,
                                                double        latitude);
double       atrebas_map_view_get_longitude        (AtrebasMapView   *view);
void         atrebas_map_view_set_longitude        (AtrebasMapView   *view,
                                                double        longitude);
double       atrebas_map_view_get_zoom             (AtrebasMapView   *view);
void         atrebas_map_view_set_zoom             (AtrebasMapView   *view,
                                                double        zoom);
GListModel * atrebas_map_view_get_layers           (AtrebasMapView   *view);
void         atrebas_map_view_set_place            (AtrebasMapView   *view,
                                                GeocodePlace *place);
void         atrebas_map_view_set_current_location (AtrebasMapView   *view,
                                                double        latitude,
                                                double        longitude,
                                                double        accuracy);
void         atrebas_map_view_set_focused_location (AtrebasMapView   *view,
                                                double        latitude,
                                                double        longitude,
                                                double        accuracy);
void         atrebas_map_view_clear                (AtrebasMapView   *view);


/**
 * ATREBAS_MAP_VIEW_MIN_ZOOM: (value 2.0)
 *
 * The minimum zoom supported by #AtrebasMapView
 */
#define ATREBAS_MAP_VIEW_MIN_ZOOM  2.0

/**
 * ATREBAS_MAP_VIEW_MAX_ZOOM: (value 20.0)
 *
 * The maximum zoom supported by #AtrebasMapView
 */
#define ATREBAS_MAP_VIEW_MAX_ZOOM 20.0

/**
 * ATREBAS_MAP_VIEW_DEFAULT_ZOOM: (value 7.0)
 *
 * The minimum zoom supported by #AtrebasMapView
 */
#define ATREBAS_MAP_VIEW_DEFAULT_ZOOM 7.0

G_END_DECLS
