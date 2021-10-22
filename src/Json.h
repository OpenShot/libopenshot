/**
 * @file
 * @brief Header file for JSON class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_JSON_H
#define OPENSHOT_JSON_H

#include <string>
#include "json/json.h"


namespace openshot {
    const Json::Value stringToJson(const std::string value);
}

#endif
