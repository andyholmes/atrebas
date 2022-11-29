// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <shumate/shumate.h>

#include "atrebas-feature.h"

G_BEGIN_DECLS

#define ATREBAS_TYPE_LEGEND_ROW (atrebas_legend_row_get_type())

G_DECLARE_FINAL_TYPE (AtrebasLegendRow, atrebas_legend_row, ATREBAS, LEGEND_ROW, GtkListBoxRow)

GtkWidget    * atrebas_legend_row_new       (ShumateLayer *layer);
ShumateLayer * atrebas_legend_row_get_layer (AtrebasLegendRow *row);
void           atrebas_legend_row_set_layer (AtrebasLegendRow *row,
                                         ShumateLayer *layer);
void           atrebas_legend_row_update    (AtrebasLegendRow *row);

G_END_DECLS
