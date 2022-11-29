// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ATREBAS_TYPE_LEGEND (atrebas_legend_get_type())

G_DECLARE_FINAL_TYPE (AtrebasLegend, atrebas_legend, ATREBAS, LEGEND, GtkBox)

GtkWidget  * atrebas_legend_new        (GListModel *layers);
GListModel * atrebas_legend_get_layers (AtrebasLegend  *legend);
void         atrebas_legend_set_layers (AtrebasLegend  *legend,
                                    GListModel *layers);

G_END_DECLS
