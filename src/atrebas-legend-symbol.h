// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

#include "atrebas-feature.h"

G_BEGIN_DECLS

#define ATREBAS_TYPE_LEGEND_SYMBOL (atrebas_legend_symbol_get_type())

G_DECLARE_FINAL_TYPE (AtrebasLegendSymbol, atrebas_legend_symbol, ATREBAS, LEGEND_SYMBOL, GtkWidget)

GtkWidget  * atrebas_legend_symbol_new         (AtrebasFeature    *feature);
AtrebasFeature * atrebas_legend_symbol_get_feature (AtrebasLegendSymbol *button);
void         atrebas_legend_symbol_set_feature (AtrebasLegendSymbol *button,
                                            AtrebasFeature    *feature);

G_END_DECLS
