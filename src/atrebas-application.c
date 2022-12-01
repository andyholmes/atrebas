// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "atrebas-application.h"
#include "atrebas-application-credits.h"
#include "atrebas-utils.h"
#include "atrebas-window.h"

/*
 * geo URI pattern.
 */
#define GEO_URI "geo:(?://)?([+-]?[0-9]{1,2}(?:\\.[0-9]+)?),([+-]?[0-9]{1,3}(?:\\.[0-9]+)?)(?:;.*)*"


struct _AtrebasApplication
{
  GtkApplication  parent_instance;

  GSettings      *settings;
  GtkWindow      *window;
};

G_DEFINE_TYPE (AtrebasApplication, atrebas_application, GTK_TYPE_APPLICATION)


/*
 * GSettings
 */
static void
on_background_changed (GSettings    *settings,
                       const char   *key,
                       GApplication *application)
{
  g_assert (G_IS_SETTINGS (settings));
  g_assert (G_IS_APPLICATION (application));

  if (g_settings_get_boolean (settings, key))
    g_application_hold (application);
  else
    g_application_release (application);
}


/*
 * GActions
 */
static void
about_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  GtkApplication *application = GTK_APPLICATION (user_data);
  GtkWindow *dialog = NULL;
  GtkWindow *window = NULL;

  g_assert (GTK_IS_APPLICATION (application));

  window = gtk_application_get_active_window (application);
  dialog = g_object_new (ADW_TYPE_ABOUT_WINDOW,
                         "application-icon",   APPLICATION_ID,
                         "application-name",   _("Atrebas"),
                         "copyright",          "© 2022 Andy Holmes",
                         "issue-url",          PACKAGE_BUGREPORT,
                         "license-type",       GTK_LICENSE_GPL_2_0,
                         "developers",         atrebas_application_credits_developers,
                         "transient-for",      window,
                         "translator-credits", _("translator-credits"),
                         "version",            PACKAGE_VERSION,
                         "website",            PACKAGE_URL,
                         NULL);
  
  for (unsigned int i = 0; i < G_N_ELEMENTS (atrebas_application_credits_legal); i++)
    {
      adw_about_window_add_legal_section (ADW_ABOUT_WINDOW (dialog),
                                          atrebas_application_credits_legal[i].title,
                                          atrebas_application_credits_legal[i].copyright,
                                          atrebas_application_credits_legal[i].license_type,
                                          atrebas_application_credits_legal[i].license);
      
    }

  gtk_window_present (dialog);
}

static void
disclaimer_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  AtrebasApplication *self = ATREBAS_APPLICATION (user_data);
  GtkWindow *dialog = NULL;
  GtkWindow *window = NULL;
  GtkWidget *checkbutton;

  g_assert (ATREBAS_IS_APPLICATION (self));

  if (!g_settings_get_boolean (self->settings, "show-disclaimer"))
    return;

  checkbutton = g_object_new (GTK_TYPE_CHECK_BUTTON,
                              "label",         _("_Don't show again"),
                              "margin-top",    24,
                              "use-underline", TRUE,
                              NULL);
  g_settings_bind (self->settings, "show-disclaimer",
                   checkbutton,    "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

  window = gtk_application_get_active_window (GTK_APPLICATION (user_data));
  dialog = g_object_new (ADW_TYPE_MESSAGE_DIALOG,
                         "heading",         _("Disclaimer"),
                         "body",            "This map does not represent or intend to represent official or legal boundaries of any Indigenous nations. To learn about definitive boundaries, contact the nations in question.\n\n"
                                            "Also, this map is not perfect — it is a work in progress with tons of contributions from the community. Please send us fixes if you find errors.\n\n"
                                            "We strive to represent nations and Indigenous people on their own terms. When there are conflicts or issues with our information, we try to fix things as soon as possible with the input of all parties involved.\n\n"
                                            "Visit <a href=\"https://native-land.ca/about/how-it-works/\">Native Land Digital</a> for more information."
                         "body-use-markup", TRUE,
                         "extra-child",     checkbutton,
                         "modal",           window != NULL,
                         "transient-for",   window,
                         NULL);
  adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog),
                                   "accept",
                                   _("_Accept"));

  gtk_window_present (dialog);
}

static void
atrebas_backend_update_cb (AtrebasBackend     *backend,
                       GAsyncResult   *result,
                       AtrebasApplication *self)
{
  GAction *action;
  g_autoptr (GError) error = NULL;

  g_assert (ATREBAS_IS_BACKEND (backend));
  g_assert (ATREBAS_IS_APPLICATION (self));

  if (!atrebas_backend_update_finish (backend, result, &error))
    {
      g_autoptr (GNotification) notification = NULL;

      g_warning ("Error updating: %s", error->message);

      notification = g_notification_new (_("Error updating"));
      g_notification_set_body (notification, error->message);
      g_application_send_notification (G_APPLICATION (self),
                                       "update-error",
                                       notification);
    }

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "update");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  g_application_unmark_busy (G_APPLICATION (self));
}

static void
update_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  AtrebasApplication *self = ATREBAS_APPLICATION (user_data);

  g_assert (ATREBAS_IS_APPLICATION (user_data));

  g_application_mark_busy (G_APPLICATION (self));
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  atrebas_backend_update (ATREBAS_BACKEND (atrebas_backend_get_default ()),
                      NULL,
                      (GAsyncReadyCallback)atrebas_backend_update_cb,
                      self);
}

static void
quit_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  GApplication *application = G_APPLICATION (user_data);

  g_assert (G_IS_APPLICATION (application));

  g_application_quit (application);
}

static const GActionEntry actions[] = {
  { "about",      about_action,      NULL,  NULL, NULL },
  { "disclaimer", disclaimer_action, NULL,  NULL, NULL },
  { "update",     update_action,     NULL,  NULL, NULL },
  { "quit",       quit_action,       NULL,  NULL, NULL }
};

/*
 * GApplication
 */
static void
atrebas_application_activate (GApplication *application)
{
  AtrebasApplication *self = ATREBAS_APPLICATION (application);

  g_assert (ATREBAS_IS_APPLICATION (self));

  if (self->window == NULL)
    {
      self->window = g_object_new (ATREBAS_TYPE_WINDOW,
                                   "application", application,
                                   NULL);
      g_object_add_weak_pointer (G_OBJECT (self->window),
                                 (gpointer) &self->window);
    }

  gtk_window_present_with_time (self->window, GDK_CURRENT_TIME);
  g_action_group_activate_action (G_ACTION_GROUP (self), "disclaimer", NULL);
}

static void
atrebas_application_open (GApplication  *application,
                      GFile        **files,
                      int            n_files,
                      const char    *hint)
{
  static GRegex *geo_uri = NULL;
  AtrebasApplication *self = ATREBAS_APPLICATION (application);
  g_autoptr (GMatchInfo) match = NULL;
  g_autofree char *uri = NULL;

  g_assert (ATREBAS_IS_APPLICATION (self));

  if (geo_uri == NULL)
    geo_uri = g_regex_new (GEO_URI, G_REGEX_OPTIMIZE, 0, NULL);

  uri = g_file_get_uri (files[0]);

  if (g_regex_match (geo_uri, uri, 0, &match))
    {
      g_autofree char *latitude_str = NULL;
      g_autofree char *longitude_str = NULL;
      double latitude, longitude;

      latitude_str = g_match_info_fetch (match, 1);
      longitude_str = g_match_info_fetch (match, 2);

      latitude = g_strtod (latitude_str, NULL);
      longitude = g_strtod (longitude_str, NULL);

      atrebas_application_activate (application);
      gtk_widget_activate_action (GTK_WIDGET (self->window),
                                  "win.location",
                                  "(dd)",
                                  latitude,
                                  longitude);
    }
}

static void
atrebas_application_startup (GApplication *application)
{
  AtrebasApplication *self = ATREBAS_APPLICATION (application);
  g_autoptr (GtkCssProvider) provider = NULL;

  g_assert (ATREBAS_IS_APPLICATION (application));

  /* Chain-up first */
  G_APPLICATION_CLASS (atrebas_application_parent_class)->startup (application);

  gtk_window_set_default_icon_name (APPLICATION_ID);
  atrebas_ui_init ();

  /* Service Actions */
  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   application);

  /* CSS */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
                                       "/ca/andyholmes/Atrebas/css/atrebas.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* Run in background */
  self->settings = g_settings_new ("ca.andyholmes.Atrebas");
  g_signal_connect (self->settings,
                    "changed::background",
                    G_CALLBACK (on_background_changed),
                    self);

  if (g_settings_get_boolean (self->settings, "background"))
    g_application_hold (application);
}

static void
atrebas_application_shutdown (GApplication *application)
{
  AtrebasApplication *self = ATREBAS_APPLICATION (application);

  g_assert (ATREBAS_IS_APPLICATION (self));

  g_clear_pointer (&self->window, gtk_window_destroy);
  g_clear_object (&self->settings);

  G_APPLICATION_CLASS (atrebas_application_parent_class)->shutdown (application);
}


/*
 * GObject
 */
static void
atrebas_application_class_init (AtrebasApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  application_class->activate = atrebas_application_activate;
  application_class->open = atrebas_application_open;
  application_class->startup = atrebas_application_startup;
  application_class->shutdown = atrebas_application_shutdown;
}

static void
atrebas_application_init (AtrebasApplication *self)
{
}

AtrebasApplication *
_atrebas_application_new (void)
{
  return g_object_new (ATREBAS_TYPE_APPLICATION,
                       "application-id",     APPLICATION_ID,
                       "resource-base-path", "/ca/andyholmes/Atrebas",
                       "flags",              G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

