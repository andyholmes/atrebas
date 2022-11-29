// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ATREBAS_TYPE_WINDOW (atrebas_window_get_type())

G_DECLARE_FINAL_TYPE (AtrebasWindow, atrebas_window, ATREBAS, WINDOW, AdwApplicationWindow)

G_END_DECLS
