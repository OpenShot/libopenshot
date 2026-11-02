/**
 * @file
 * @brief Qt-based JSON parsing and serialization helpers
 *
 * Copyright (c) 2026 OpenShot Studios, LLC
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "QtJsonAdapter.h"
#include "Exceptions.h"

#include <QByteArray>
#include <QJsonParseError>

namespace openshot {
namespace detail {

QJsonDocument parseQtJson(const std::string& value)
{
    QJsonParseError error;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            QByteArray::fromStdString(value),
            &error);

    if (error.error != QJsonParseError::NoError ||
        document.isNull()) {
        throw openshot::InvalidJSON(
            "JSON could not be parsed (or is invalid)");
    }

    return document;
}

std::string serializeQtJson(
    const QJsonDocument& document,
    bool compact)
{
    const QJsonDocument::JsonFormat format =
        compact
            ? QJsonDocument::Compact
            : QJsonDocument::Indented;

    return document.toJson(format).toStdString();
}

} // namespace detail
} // namespace openshot
