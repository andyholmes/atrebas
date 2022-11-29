// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <shumate/shumate.h>
#include <gtk/gtk.h>

#include "atrebas-feature.h"

G_BEGIN_DECLS

#define ATREBAS_TYPE_FEATURE_LAYER (atrebas_feature_layer_get_type())

G_DECLARE_FINAL_TYPE (AtrebasFeatureLayer, atrebas_feature_layer, ATREBAS, FEATURE_LAYER, ShumateLayer)

GtkWidget  * atrebas_feature_layer_new         (ShumateViewport *viewport,
                                            AtrebasFeature      *feature);
AtrebasFeature * atrebas_feature_layer_get_feature (AtrebasFeatureLayer *layer);

G_END_DECLS
