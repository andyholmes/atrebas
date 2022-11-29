// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

#include "atrebas-backend.h"

G_BEGIN_DECLS

#define ATREBAS_TYPE_SEARCH_MODEL (atrebas_search_model_get_type())

G_DECLARE_FINAL_TYPE (AtrebasSearchModel, atrebas_search_model, ATREBAS, SEARCH_MODEL, GObject)

GListModel     * atrebas_search_model_new           (GeocodeBackend *backend);
GeocodeBackend * atrebas_search_model_get_backend   (AtrebasSearchModel *search);
const char     * atrebas_search_model_get_query     (AtrebasSearchModel *search);
void             atrebas_search_model_set_query     (AtrebasSearchModel *search,
                                                 const char     *query);
double           atrebas_search_model_get_latitude  (AtrebasSearchModel *search);
void             atrebas_search_model_set_latitude  (AtrebasSearchModel *search,
                                                 double          latitude);
double           atrebas_search_model_get_longitude (AtrebasSearchModel *search);
void             atrebas_search_model_set_longitude (AtrebasSearchModel *search,
                                                 double          longitude);

G_END_DECLS

