// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <shumate/shumate.h>

G_BEGIN_DECLS

#define MOCK_TYPE_MAP_SOURCE (mock_map_source_get_type ())

G_DECLARE_DERIVABLE_TYPE (MockMapSource, mock_map_source, MOCK, MAP_SOURCE, ShumateMapSource)

struct _MockMapSourceClass
{
  ShumateMapSourceClass parent_class;
};

ShumateMapSource * mock_map_source_new (void);

G_END_DECLS

