/**
 * @file
 * @brief Qt JSON compatibility investigation for issue #142
 *
 * @ref License
 */

// Copyright (c) 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Json.h"
#include "Exceptions.h"
#include "openshot_catch.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <string>

namespace {

QJsonDocument parseQt(const std::string& input)
{
    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(
            QByteArray::fromStdString(input), &error);

    if (error.error != QJsonParseError::NoError)
        throw openshot::InvalidJSON(
            "JSON could not be parsed (or is invalid)");

    return document;
}

} // namespace

TEST_CASE("Qt JSON parses nested objects and arrays",
          "[json][qtjson]")
{
    const std::string input =
        R"({"clip":{"id":"abc","frames":[1,2,3]}})";

    const Json::Value legacy =
        openshot::stringToJson(input);

    const QJsonObject modern =
        parseQt(input).object();

    CHECK(legacy["clip"]["id"].asString() == "abc");
    CHECK(modern.value("clip").toObject()
                .value("id").toString() == "abc");

    CHECK(legacy["clip"]["frames"].size() == 3);
    CHECK(modern.value("clip").toObject()
                .value("frames").toArray().size() == 3);
}

TEST_CASE("Qt JSON preserves Unicode strings",
          "[json][qtjson]")
{
    const std::string input =
        u8R"({"text":"Krishna \u0915\u0943\u0937\u094d\u0923"})";

    const Json::Value legacy =
        openshot::stringToJson(input);

    const QJsonObject modern =
        parseQt(input).object();

    CHECK(QString::fromStdString(
              legacy["text"].asString()) ==
          modern.value("text").toString());
}

TEST_CASE("Qt JSON preserves boolean and null types",
          "[json][qtjson]")
{
    const std::string input =
        R"({"enabled":true,"missing":null})";

    const Json::Value legacy =
        openshot::stringToJson(input);

    const QJsonObject modern =
        parseQt(input).object();

    CHECK(legacy["enabled"].asBool());
    CHECK(modern.value("enabled").toBool());

    CHECK(legacy["missing"].isNull());
    CHECK(modern.value("missing").isNull());
}

TEST_CASE("Qt JSON distinguishes empty arrays and objects",
          "[json][qtjson]")
{
    const std::string input =
        R"({"array":[],"object":{}})";

    const Json::Value legacy =
        openshot::stringToJson(input);

    const QJsonObject modern =
        parseQt(input).object();

    CHECK(legacy["array"].isArray());
    CHECK(legacy["object"].isObject());

    CHECK(modern.value("array").isArray());
    CHECK(modern.value("object").isObject());
}

TEST_CASE("Qt JSON rejects malformed input",
          "[json][qtjson]")
{
    const std::string input =
        R"({"clip":[1,2,})";

    CHECK_THROWS_AS(
        openshot::stringToJson(input),
        openshot::InvalidJSON);

    CHECK_THROWS_AS(
        parseQt(input),
        openshot::InvalidJSON);
}

TEST_CASE("Qt JSON preserves ordinary numeric values",
          "[json][qtjson]")
{
    const std::string input =
        R"({"frame":120,"opacity":0.75})";

    const Json::Value legacy =
        openshot::stringToJson(input);

    const QJsonObject modern =
        parseQt(input).object();

    CHECK(legacy["frame"].asInt() ==
          modern.value("frame").toInt());

    CHECK(legacy["opacity"].asDouble() ==
          Approx(modern.value("opacity").toDouble()));
}
