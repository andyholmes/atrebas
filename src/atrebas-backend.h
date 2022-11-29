// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

#include "atrebas-feature.h"

G_BEGIN_DECLS

#define ATREBAS_TYPE_BACKEND (atrebas_backend_get_type())

G_DECLARE_FINAL_TYPE (AtrebasBackend, atrebas_backend, ATREBAS, BACKEND, GObject)

GeocodeBackend * atrebas_backend_new           (const char           *path);
GeocodeBackend * atrebas_backend_get_default   (void);
const char     * atrebas_backend_get_path      (AtrebasBackend       *backend);
void             atrebas_backend_load          (AtrebasBackend       *backend,
                                                const char           *filename,
                                                AtrebasMapTheme       theme,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean         atrebas_backend_load_finish   (AtrebasBackend       *backend,
                                                GAsyncResult         *result,
                                                GError              **error);
void             atrebas_backend_lookup        (AtrebasBackend       *backend,
                                                const char           *id,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
AtrebasFeature * atrebas_backend_lookup_finish (AtrebasBackend       *backend,
                                                GAsyncResult         *result,
                                                GError              **error);
void             atrebas_backend_update        (AtrebasBackend       *backend,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean         atrebas_backend_update_finish (AtrebasBackend       *backend,
                                                GAsyncResult         *result,
                                                GError              **error);

/* Utilities */
GHashTable *     atrebas_geocode_parameters_for_coordinates (double        latitude,
                                                             double        longitude);
GHashTable *     atrebas_geocode_parameters_for_location    (const char   *location);

G_END_DECLS
