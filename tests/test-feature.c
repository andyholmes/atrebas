// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>


#include "mock-common.h"


static void
test_feature_new (void)
{
  AtrebasFeature *feature = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) coordinates = NULL;

  /* Build coordinates */
  builder = json_builder_new ();
  json_builder_begin_array (builder);
  json_builder_begin_array (builder);

  json_builder_begin_array (builder);
  json_builder_add_double_value (builder, 0.0);
  json_builder_add_double_value (builder, 0.0);
  json_builder_end_array (builder);

  json_builder_begin_array (builder);
  json_builder_add_double_value (builder, 0.0);
  json_builder_add_double_value (builder, 5.0);
  json_builder_end_array (builder);

  json_builder_begin_array (builder);
  json_builder_add_double_value (builder, 2.5);
  json_builder_add_double_value (builder, 2.5);
  json_builder_end_array (builder);

  json_builder_end_array (builder);
  json_builder_end_array (builder);
  coordinates = json_builder_get_root (builder);

  feature = atrebas_feature_new ("name", "uri", json_node_get_array (coordinates));
  g_assert_finalize_object (feature);
}

static void
test_feature_basic (void)
{
  AtrebasFeature *feature = NULL;
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (JsonNode) node = NULL;
  g_autofree char *color = NULL;
  g_autofree char *nld_id = NULL;
  g_autofree char *name = NULL;
  g_autofree char *name_fr = NULL;
  g_autofree char *slug = NULL;
  g_autofree char *uri = NULL;
  g_autofree char *uri_fr = NULL;
  g_autoptr (JsonArray) coordinates = NULL;
  AtrebasMapTheme theme;
  GError *error = NULL;

  /* Load the JSON */
  parser = json_parser_new ();
  json_parser_load_from_file (parser, TEST_DATA_DIR"/testFeature.json", NULL);
  node = json_parser_steal_root (parser);

  /* Deserialize the feature */
  feature = atrebas_feature_deserialize (node, &error);
  g_assert_no_error (error);
  g_assert_true (ATREBAS_IS_FEATURE (feature));

  g_object_get (feature,
                "nld-id",      &nld_id,
                "name",        &name,
                "name-fr",     &name_fr,
                "uri",         &uri,
                "uri-fr",      &uri_fr,
                "color",       &color,
                "coordinates", &coordinates,
                "slug",        &slug,
                "theme",       &theme,
                NULL);

  /* id should be set, but everything else should be default or NULL */
  g_assert_cmpstr (nld_id, ==, ATREBAS_TEST_FEATURE_ID);
  g_assert_cmpstr (name, ==, ATREBAS_TEST_FEATURE_NAME);
  g_assert_cmpstr (name_fr, ==, ATREBAS_TEST_FEATURE_NAME);
  g_assert_cmpstr (uri, ==, "https://native-land.ca/maps/territories/zacateco/");
  g_assert_cmpstr (uri_fr, ==, "https://native-land.ca/maps/territories/zacateco/");
  g_assert_cmpstr (color, ==, "#389a2a");
  g_assert_nonnull (coordinates);
  g_assert_cmpstr (slug, ==, "zacateco");
  g_assert_cmpuint (theme, ==, ATREBAS_MAP_THEME_TERRITORY);

  g_assert_cmpstr (atrebas_feature_get_nld_id (feature), ==, ATREBAS_TEST_FEATURE_ID);
  g_assert_cmpstr (atrebas_feature_get_name_fr (feature), ==, ATREBAS_TEST_FEATURE_NAME);
  g_assert_cmpstr (atrebas_feature_get_uri (feature), ==, "https://native-land.ca/maps/territories/zacateco/");
  g_assert_cmpstr (atrebas_feature_get_uri_fr (feature), ==, "https://native-land.ca/maps/territories/zacateco/");
  g_assert_cmpstr (atrebas_feature_get_color (feature), ==, "#389a2a");
  g_assert_true (atrebas_feature_get_coordinates (feature) == coordinates);
  g_assert_cmpstr (atrebas_feature_get_slug (feature), ==, "zacateco");
  g_assert_cmpuint (atrebas_feature_get_theme (feature), ==, ATREBAS_MAP_THEME_TERRITORY);

  g_assert_finalize_object (feature);
}

static void
test_feature_misc (void)
{
  g_autoptr (AtrebasFeature) feature1 = NULL;
  g_autoptr (AtrebasFeature) feature2 = NULL;
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (JsonNode) feature = NULL;

  /* Load the JSON */
  parser = json_parser_new ();
  json_parser_load_from_file (parser, TEST_DATA_DIR"/testFeature.json", NULL);
  feature = json_parser_steal_root (parser);

  /* Compare features for equality */
  feature1 = atrebas_feature_deserialize (feature, NULL);
  feature2 = atrebas_feature_deserialize (feature, NULL);
  g_assert_true (atrebas_feature_equal (feature1, feature2));
  g_assert_cmpuint (atrebas_feature_hash (feature1), ==, atrebas_feature_hash (feature2));

  /* Check if a point is inside a feature */
  g_assert_true (atrebas_feature_contains_point (feature1,
                                             ATREBAS_TEST_FEATURE_LAT,
                                             ATREBAS_TEST_FEATURE_LON));
  g_assert_false (atrebas_feature_contains_point (feature1, 0.0, 0.0));
}

static void
test_feature_serialize (void)
{

  AtrebasFeature *feature1 = NULL;
  AtrebasFeature *feature2 = NULL;
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (JsonNode) feature = NULL;
  GError *error = NULL;

  /* Load the serialized feature */
  parser = json_parser_new ();
  json_parser_load_from_file (parser, TEST_DATA_DIR"/testFeature.json", NULL);
  feature = json_parser_steal_root (parser);
  g_assert_true (JSON_NODE_HOLDS_OBJECT (feature));

  /* Deserialize the feature */
  feature1 = atrebas_feature_deserialize (feature, &error);
  g_assert_no_error (error);
  g_assert_true (ATREBAS_IS_FEATURE (feature1));

  /* Serialize the feature */
  g_clear_pointer (&feature, json_node_unref);
  feature = atrebas_feature_serialize (feature1);
  g_assert_true (JSON_NODE_HOLDS_OBJECT (feature));

  /* Deserialize the feature */
  feature2 = atrebas_feature_deserialize (feature, &error);
  g_assert_no_error (error);
  g_assert_true (ATREBAS_IS_FEATURE (feature2));

  /* Test equality */
  g_assert_true (atrebas_feature_equal (feature1, feature2));

  g_assert_finalize_object (feature1);
  g_assert_finalize_object (feature2);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/atrebas/feature/new",
                   test_feature_new);
  g_test_add_func ("/atrebas/feature/basic",
                   test_feature_basic);
  g_test_add_func ("/atrebas/feature/misc",
                   test_feature_misc);
  g_test_add_func ("/atrebas/feature/serialize",
                   test_feature_serialize);

  return g_test_run ();
}

