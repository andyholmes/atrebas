// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib/gi18n.h>

#include "atrebas-application.h"


int
main (int   argc,
      char *argv[])
{
  g_autoptr (AtrebasApplication) application = NULL;
  int ret;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_set_application_name (_("Atrebas"));

  application = _atrebas_application_new ();
  ret = g_application_run (G_APPLICATION (application), argc, argv);

  return ret;
}
