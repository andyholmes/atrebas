// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once


/**
 * ATREBAS_BACKEND_TABLES_SQL:
 *
 * @id: (type utf8): The unique identifier
 * @name: (type utf8): The feature name
 * @name_fr: (type utf8): The feature name (French)
 * @description: (type utf8): The feature description
 * @description_fr: (type utf8): The feature description (French)
 * @color: (type utf8): A hex color (eg. #FFCC00)
 * @coordinate: (type utf8): The bounding coordinates
 * @slug: (type utf8): The unique identifier
 * @theme: a #AtrebasMapTheme
 *
 * The SQL query used to create the `language`, `territory` and treaty` table,
 * which holds records of their respective features. All field are text, with the
 * caveats of @color being a hex color code, and @coordinates being a
 * stringified JSON array of bounding coordinates for the feature.
 */
#define ATREBAS_BACKEND_FEATURE_TABLE_SQL           \
"CREATE TABLE IF NOT EXISTS feature ("          \
"  id               TEXT PRIMARY KEY NOT NULL," \
"  name             TEXT             NOT NULL," \
"  name_fr          TEXT             NOT NULL," \
"  description      TEXT             NOT NULL," \
"  description_fr   TEXT             NOT NULL," \
"  color            TEXT             NOT NULL," \
"  coordinates      TEXT             NOT NULL," \
"  slug             TEXT             NOT NULL," \
"  theme            INTEGER"                    \
");"


/**
 * ADD_FEATURE_SQL:
 *
 * Insert or update a language feature.
 */
#define ADD_FEATURE_SQL                                                                        \
"INSERT INTO feature(id,name,name_fr,description,description_fr,color,coordinates,slug,theme)" \
"  VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"                                                         \
"  ON CONFLICT(id) DO UPDATE SET"                                                              \
"    name=excluded.name,"                                                                      \
"    name_fr=excluded.name_fr,"                                                                \
"    description=excluded.description,"                                                        \
"    description_fr=excluded.description_fr,"                                                  \
"    color=excluded.color,"                                                                    \
"    coordinates=excluded.coordinates,"                                                        \
"    slug=excluded.slug,"                                                                      \
"    theme=excluded.theme;"


/**
 * REMOVE_FEATURE_SQL:
 *
 * Remove the language feature for `id`.
 */
#define REMOVE_FEATURE_SQL \
"DELETE FROM feature"      \
"  WHERE id=?;"


/**
 * SEARCH_FEATURES_SQL:
 *
 * Search features by name.
 */
#define SEARCH_FEATURES_SQL   \
"SELECT * FROM feature"       \
"  WHERE name LIKE ? LIMIT ?"


/**
 * GET_FEATURES_SQL:
 *
 * Search features by name.
 */
#define GET_FEATURES_SQL \
"SELECT * FROM feature"


/**
 * GET_FEATURE_SQL:
 *
 * Get the language feature for `id`.
 */
#define GET_FEATURE_SQL \
"SELECT * FROM feature" \
"  WHERE id=?;"

