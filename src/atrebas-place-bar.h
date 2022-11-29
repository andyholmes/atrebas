// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <geocode-glib/geocode-glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ATREBAS_TYPE_PLACE_BAR (atrebas_place_bar_get_type())

G_DECLARE_FINAL_TYPE (AtrebasPlaceBar, atrebas_place_bar, ATREBAS, PLACE_BAR, GtkBox)

GtkWidget    * atrebas_place_bar_new           (GeocodePlace *place);
GListModel   * atrebas_place_bar_get_features  (AtrebasPlaceBar  *bar);
double         atrebas_place_bar_get_latitude  (AtrebasPlaceBar  *bar);
void           atrebas_place_bar_set_latitude  (AtrebasPlaceBar  *bar,
                                            double        latitude);
double         atrebas_place_bar_get_longitude (AtrebasPlaceBar  *bar);
void           atrebas_place_bar_set_longitude (AtrebasPlaceBar  *bar,
                                            double        longitude);
GeocodePlace * atrebas_place_bar_get_place     (AtrebasPlaceBar  *bar);
void           atrebas_place_bar_set_place     (AtrebasPlaceBar  *bar,
                                            GeocodePlace *place);

G_END_DECLS
