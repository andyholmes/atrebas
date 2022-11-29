#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>


# Clean old files
rm -rf ${GSETTINGS_SCHEMA_DIR}
mkdir -p ${GSETTINGS_SCHEMA_DIR}

# Copy and compile GSettings schemas
cp ${GSETTINGS_SCHEMA_XML}/*.gschema.xml ${GSETTINGS_SCHEMA_DIR}
glib-compile-schemas ${GSETTINGS_SCHEMA_DIR}

