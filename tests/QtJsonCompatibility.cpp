#include <cmath>
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
#include "QtJsonAdapter.h"
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


TEST_CASE("Qt JSON adapter parses objects and arrays",
          "[json][qtjson][adapter]")
{
    const auto object = openshot::detail::parseQtJson(
        R"({"clip":{"id":"abc"},"frames":[1,2,3]})");

    CHECK(object.isObject());
    CHECK(object.object().value("clip")
              .toObject().value("id").toString() == "abc");
    CHECK(object.object().value("frames").toArray().size() == 3);

    const auto array = openshot::detail::parseQtJson(
        R"([1,2,3])");

    CHECK(array.isArray());
    CHECK(array.array().size() == 3);
}

TEST_CASE("Qt JSON adapter serializes compact and indented JSON",
          "[json][qtjson][adapter]")
{
    const auto document = openshot::detail::parseQtJson(
        R"({"enabled":true,"frame":120})");

    const std::string compact =
        openshot::detail::serializeQtJson(document, true);

    const std::string indented =
        openshot::detail::serializeQtJson(document, false);

    CHECK(compact.find('\n') == std::string::npos);
    CHECK(indented.find('\n') != std::string::npos);

    CHECK(openshot::detail::parseQtJson(compact) == document);
    CHECK(openshot::detail::parseQtJson(indented) == document);
}

TEST_CASE("Qt JSON adapter preserves round-trip values",
          "[json][qtjson][adapter]")
{
    const std::string input =
        R"({"name":"Krishna","values":[1,2.5,null,true]})";

    const auto original =
        openshot::detail::parseQtJson(input);

    const auto restored =
        openshot::detail::parseQtJson(
            openshot::detail::serializeQtJson(original, true));

    CHECK(restored == original);
    CHECK(restored.object().value("values").toArray().size() == 4);
}

TEST_CASE("Qt JSON adapter rejects invalid input",
          "[json][qtjson][adapter]")
{
    CHECK_THROWS_AS(
        openshot::detail::parseQtJson(R"({"broken":})"),
        openshot::InvalidJSON);

    CHECK_THROWS_AS(
        openshot::detail::parseQtJson(""),
        openshot::InvalidJSON);

    CHECK_THROWS_AS(
        openshot::detail::parseQtJson("null"),
        openshot::InvalidJSON);
}


TEST_CASE("Qt JSON adapter documents large integer precision",
          "[json][qtjson][adapter][precision]")
{
    const std::string input =
        R"({"value":9007199254740993})";

    const Json::Value legacy = openshot::stringToJson(input);
    const auto modern = openshot::detail::parseQtJson(input);

    REQUIRE(legacy["value"].isIntegral());
    CHECK(legacy["value"].asLargestUInt() ==
          static_cast<Json::LargestUInt>(9007199254740993ULL));

    const double qtValue =
        modern.object().value("value").toDouble();

    INFO("Qt numeric representation: " << qtValue);

    CHECK(std::isfinite(qtValue));
}

TEST_CASE("Qt JSON adapter preserves ordinary signed integers",
          "[json][qtjson][adapter][precision]")
{
    const std::string input =
        R"({"negative":-120,"positive":120})";

    const auto modern = openshot::detail::parseQtJson(input);

    CHECK(modern.object().value("negative").toInt() == -120);
    CHECK(modern.object().value("positive").toInt() == 120);
}


TEST_CASE("Qt JSON adapter reports large integer serialization",
          "[json][qtjson][adapter][precision]")
{
    const std::string input =
        R"({"value":9007199254740993})";

    const Json::Value legacy = openshot::stringToJson(input);
    const auto modern = openshot::detail::parseQtJson(input);

    const std::string legacyOutput = legacy.toStyledString();
    const std::string qtOutput =
        openshot::detail::serializeQtJson(modern, true);

    INFO("JsonCpp: " << legacyOutput);
    INFO("Qt JSON: " << qtOutput);

    CHECK(legacy["value"].asLargestUInt() ==
          static_cast<Json::LargestUInt>(9007199254740993ULL));

    CHECK_FALSE(qtOutput.empty());
}


#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
TEST_CASE("Qt5 JSON documents large integer precision loss",
          "[json][qtjson][precision]")
{
    const std::string input =
        R"({"value":9007199254740993})";

    const Json::Value legacy =
        openshot::stringToJson(input);

    const auto modern =
        openshot::detail::parseQtJson(input);

    const std::string output =
        openshot::detail::serializeQtJson(modern, true);

    REQUIRE(legacy["value"].asLargestUInt() ==
            static_cast<Json::LargestUInt>(
                9007199254740993ULL));

    CHECK(output.find("9007199254740992") !=
          std::string::npos);

    CHECK(output.find("9007199254740993") ==
          std::string::npos);
}
#endif

TEST_CASE("Compare string-valued integer conversions",
          "[json][qtjson][conversion]")
{
    const std::string input = R"({"width":"1280"})";

    const Json::Value legacy =
        openshot::stringToJson(input);

    const QJsonObject modern =
        openshot::detail::parseQtJson(input).object();

    INFO("JsonCpp width type: " << legacy["width"].type());
    INFO("Qt width type: " << modern.value("width").type());

    CHECK(modern.value("width").toInt() == 0);

    // Check whether JsonCpp rejects this conversion.
    bool legacyThrows = false;
    try {
        (void)legacy["width"].asInt();
    } catch (const std::exception&) {
        legacyThrows = true;
    }

    INFO("JsonCpp conversion throws: " << legacyThrows);
    CHECK(legacyThrows);
}

TEST_CASE("Compare out-of-range integer conversions",
          "[json][qtjson][conversion]")
{
    const std::string input =
        R"({"width":9223372036854775807})";

    const Json::Value legacy =
        openshot::stringToJson(input);

    const QJsonObject modern =
        openshot::detail::parseQtJson(input).object();

    CHECK(modern.value("width").toInt() == 0);

    bool legacyThrows = false;
    try {
        (void)legacy["width"].asInt();
    } catch (const std::exception&) {
        legacyThrows = true;
    }

    INFO("JsonCpp out-of-range conversion throws: "
         << legacyThrows);

    CHECK(legacyThrows);
}
