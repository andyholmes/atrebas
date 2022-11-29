// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

static const char *atrebas_application_credits_developers[] = {
  "Andy Holmes",
  NULL
};

static struct
{
  const char *title;
  const char *copyright;
  GtkLicense  license_type;
  const char *license;
} atrebas_application_credits_legal[] = {
    {
      N_("Cultural Data"),
      "© <a href=\"https://www.native-land.c\">Native Land Digital</a>",
      GTK_LICENSE_CUSTOM,
      "<a href=\"https://creativecommons.org/publicdomain/zero/1.0/legalcode\">Creative Commons Zero v1.0 Universal (CC0-1.0)</a>"
    },
    {
      N_("Map Data"),
      "© <a href=\"https://www.openstreetmap.org\">OpenStreetMap</a> contributors",
      GTK_LICENSE_CUSTOM,
      "<a href=\"https://opendatacommons.org/licenses/odbl/1-0/\">Open Data Commons Open Database License v1.0 (ODbL-1.0)</a>"
    },
    {
      N_("Map Imagery"),
      "© <a href=\"https://www.openstreetmap.org\">OpenStreetMap</a>",
      GTK_LICENSE_CUSTOM,
      "<a href=\"https://creativecommons.org/licenses/by-sa/2.0/legalcode\">Creative Commons Attribution Share Alike 2.0 Generic (CC-BY-SA-2.0)</a>"
    }
};

G_END_DECLS

