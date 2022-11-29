// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <geocode-glib/geocode-glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ATREBAS_TYPE_PLACE_HEADER (atrebas_place_header_get_type())

G_DECLARE_FINAL_TYPE (AtrebasPlaceHeader, atrebas_place_header, ATREBAS, PLACE_HEADER, GtkBox)

GtkWidget    * atrebas_place_header_new       (GeocodePlace   *place);
GeocodePlace * atrebas_place_header_get_place (AtrebasPlaceHeader *header);
void           atrebas_place_header_set_place (AtrebasPlaceHeader *header,
                                           GeocodePlace   *place);

G_END_DECLS
