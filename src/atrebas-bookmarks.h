// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define ATREBAS_TYPE_BOOKMARKS (atrebas_bookmarks_get_type())

G_DECLARE_FINAL_TYPE (AtrebasBookmarks, atrebas_bookmarks, ATREBAS, BOOKMARKS, GObject)

GListModel * atrebas_bookmarks_get_default  (void);
void         atrebas_bookmarks_add_place    (AtrebasBookmarks *bookmarks,
                                         GeocodePlace *place);
void         atrebas_bookmarks_remove_place (AtrebasBookmarks *bookmarks,
                                         GeocodePlace *place);
gboolean     atrebas_bookmarks_has_place    (AtrebasBookmarks *bookmarks,
                                         GeocodePlace *place);

G_END_DECLS

