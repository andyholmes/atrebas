// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "atrebas-feature.h"
#include "atrebas-macros.h"

#include "atrebas.h"

G_BEGIN_DECLS

char       * atrebas_geocode_place_address (GeocodePlace *place);
char       * atrebas_geocode_place_area    (GeocodePlace *place);
const char * atrebas_map_theme_icon        (AtrebasMapTheme theme);
const char * atrebas_map_theme_name        (AtrebasMapTheme theme);
gboolean     atrebas_ui_init               (void);

G_END_DECLS

