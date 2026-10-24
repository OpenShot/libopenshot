/**
 * @file
 * @brief Qt-based JSON parsing and serialization helpers
 *
 * Copyright (c) 2026 OpenShot Studios, LLC
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef OPENSHOT_QT_JSON_ADAPTER_H
#define OPENSHOT_QT_JSON_ADAPTER_H

#include <QJsonDocument>
#include <QtGlobal>
#include <string>

namespace openshot {
namespace detail {

/**
 * Parse a UTF-8 JSON object or array using Qt.
 *
 * Throws InvalidJSON when parsing fails.
 *
 * Note: QJsonDocument supports object and array roots.
 */
Q_DECL_HIDDEN QJsonDocument parseQtJson(const std::string& value);

/**
 * Serialize a Qt JSON document as UTF-8.
 *
 * The compact flag selects compact or indented output.
 */
Q_DECL_HIDDEN std::string serializeQtJson(
    const QJsonDocument& document,
    bool compact = false);

} // namespace detail
} // namespace openshot

#endif
