// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ATREBAS_TYPE_APPLICATION (atrebas_application_get_type())

G_DECLARE_FINAL_TYPE (AtrebasApplication, atrebas_application, ATREBAS, APPLICATION, GtkApplication)

AtrebasApplication * _atrebas_application_new (void);

G_END_DECLS
