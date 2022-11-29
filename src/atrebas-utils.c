// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "atrebas-utils"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "atrebas-utils.h"


/**
 * SECTION:atrebasutils
 * @short_description: Miscellaneous utilities
 * @title: Utilities
 * @stability: Unstable
 * @include: atrebas.h
 *
 * A collection of miscellaneous utilities.
 */


/**
 * atrebas_map_theme_icon:
 * @theme: a #AtrebasMapTheme
 *
 * Get an icon name for @theme.
 *
 * Returns: (transfer none): an icon name
 */
const char *
atrebas_map_theme_icon (AtrebasMapTheme theme)
{
  if (theme == ATREBAS_MAP_THEME_LANGUAGE)
    return "atrebas-language-symbolic";

  if (theme == ATREBAS_MAP_THEME_TERRITORY)
    return "atrebas-territory-symbolic";

  if (theme == ATREBAS_MAP_THEME_TREATY)
    return "atrebas-treaty-symbolic";

  return "atrebas-map-symbolic";
}

/**
 * atrebas_map_theme_name:
 * @theme: a #AtrebasMapTheme
 *
 * Get a human readable name for @theme.
 *
 * Returns: (transfer none): a name
 */
const char *
atrebas_map_theme_name (AtrebasMapTheme theme)
{
  if (theme == ATREBAS_MAP_THEME_LANGUAGE)
    return _("Language");

  if (theme == ATREBAS_MAP_THEME_TERRITORY)
    return _("Territory");

  if (theme == ATREBAS_MAP_THEME_TREATY)
    return _("Treaty");

  return "";
}

/**
 * atrebas_geocode_place_address:
 * @place: a #GeocodePlace
 *
 * Get a multi-line address for @place.
 *
 * Returns: (transfer full): an address
 */
char *
atrebas_geocode_place_address (GeocodePlace *place)
{
  const char *name = NULL;
  const char *street = NULL;
  const char *city = NULL;
  const char *postal = NULL;
  const char *state = NULL;
  const char *country = NULL;

  g_return_val_if_fail (GEOCODE_IS_PLACE (place), NULL);

  name = geocode_place_get_name (place);
  street = geocode_place_get_street_address (place);
  city = geocode_place_get_town (place);
  postal = geocode_place_get_postal_code (place);
  state = geocode_place_get_state (place);
  country = geocode_place_get_country (place);

  if (name && street && city && postal && state && country)
    return g_strdup_printf ("<b>%s</b>\n%s\n%s %s\n%s, %s",
                            name,
                            street,
                            city, postal,
                            state, country);

  return NULL;
}

/**
 * atrebas_geocode_place_area:
 * @place: a #GeocodePlace
 *
 * Get a multi-line address for @place.
 *
 * Returns: (transfer full): an address
 */
char *
atrebas_geocode_place_area (GeocodePlace *place)
{
  const char *town = NULL;
  const char *state = NULL;
  const char *country = NULL;

  g_return_val_if_fail (GEOCODE_IS_PLACE (place), NULL);

  if (ATREBAS_IS_FEATURE (place))
    {
      AtrebasFeature *feature = ATREBAS_FEATURE (place);
      AtrebasMapTheme theme = atrebas_feature_get_theme (feature);

      return g_strdup (atrebas_map_theme_name (theme));
    }

  town = geocode_place_get_town (place);
  state = geocode_place_get_state (place);
  country = geocode_place_get_country (place);

  if (town != NULL && state != NULL && country != NULL)
    return g_strdup_printf ("%s, %s, %s", town, state, country);

  return NULL;
}

/**
 * atrebas_ui_init:
 *
 * Initialize GTK, Adwaita and then Atrebas's UI types.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 */
gboolean
atrebas_ui_init (void)
{
  static gsize guard = FALSE;
  static gboolean initialized = FALSE;

  if (g_once_init_enter (&guard))
    {
      if (!gtk_init_check ())
        {
          g_once_init_leave (&guard, TRUE);
          return initialized;
        }

      adw_init ();

      g_type_ensure (ATREBAS_TYPE_BACKEND);
      g_type_ensure (ATREBAS_TYPE_FEATURE);
      g_type_ensure (ATREBAS_TYPE_SEARCH_MODEL);
      g_type_ensure (ATREBAS_TYPE_BOOKMARKS);
      g_type_ensure (ATREBAS_TYPE_FEATURE_LAYER);
      g_type_ensure (ATREBAS_TYPE_LEGEND);
      g_type_ensure (ATREBAS_TYPE_LEGEND_ROW);
      g_type_ensure (ATREBAS_TYPE_LEGEND_SYMBOL);
      g_type_ensure (ATREBAS_TYPE_MAP_MARKER);
      g_type_ensure (ATREBAS_TYPE_MAP_VIEW);
      g_type_ensure (ATREBAS_TYPE_PLACE_BAR);
      g_type_ensure (ATREBAS_TYPE_PLACE_HEADER);
      g_type_ensure (ATREBAS_TYPE_PREFERENCES_WINDOW);
      g_type_ensure (ATREBAS_TYPE_WINDOW);

      initialized = TRUE;
      g_once_init_leave (&guard, TRUE);
    }

  return initialized;
}

