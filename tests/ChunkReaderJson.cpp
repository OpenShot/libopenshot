/**
 * @file
 * @brief ChunkReader JSON metadata regression tests
 *
 * Copyright (c) 2026 OpenShot Studios, LLC
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ChunkReader.h"
#include "Exceptions.h"
#include "openshot_catch.h"

#include <QTemporaryDir>

#include <fstream>
#include <string>

namespace {

void checkInvalidMetadata(const std::string& json)
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const std::string filename =
        directory.path().toStdString() + "/info.json";

    std::ofstream output(filename);
    REQUIRE(output.is_open());

    output << json;
    output.close();

    REQUIRE_THROWS_AS(
        openshot::ChunkReader(directory.path().toStdString(),
                              openshot::PREVIEW),
        openshot::InvalidJSON);
}

} // namespace

TEST_CASE("ChunkReader rejects malformed JSON",
          "[chunkreader][json]")
{
    checkInvalidMetadata("{invalid json");
}

TEST_CASE("ChunkReader rejects missing 64-bit metadata",
          "[chunkreader][json]")
{
    checkInvalidMetadata(R"({"width":1920,"height":1080})");
}

TEST_CASE("ChunkReader rejects invalid file_size",
          "[chunkreader][json]")
{
    checkInvalidMetadata(
        R"({"file_size":"not-a-number","video_length":"100"})");
}

TEST_CASE("ChunkReader rejects invalid video_length",
          "[chunkreader][json]")
{
    checkInvalidMetadata(
        R"({"file_size":"100","video_length":"not-a-number"})");
}

namespace {

void checkValidMetadata(const std::string& json)
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const std::string filename =
        directory.path().toStdString() + "/info.json";

    std::ofstream output(filename);
    REQUIRE(output.is_open());

    output << json;
    output.close();

    openshot::ChunkReader reader(
        directory.path().toStdString(),
        openshot::PREVIEW);

    const auto& info = reader.info;

    CHECK(info.has_video == true);
    CHECK(info.has_audio == true);
    CHECK(info.duration == 10.0);
    CHECK(info.width == 1280);
    CHECK(info.height == 720);
    CHECK(info.pixel_format == 0);

    CHECK(info.fps.num == 30);
    CHECK(info.fps.den == 1);

    CHECK(info.video_bit_rate == 2500000);
    CHECK(info.pixel_ratio.num == 1);
    CHECK(info.pixel_ratio.den == 1);
    CHECK(info.display_ratio.num == 16);
    CHECK(info.display_ratio.den == 9);

    CHECK(info.vcodec == "vp9");
    CHECK(info.video_stream_index == 0);
    CHECK(info.video_timebase.num == 1);
    CHECK(info.video_timebase.den == 30);

    CHECK(info.interlaced_frame == false);
    CHECK(info.top_field_first == false);

    CHECK(info.acodec == "opus");
    CHECK(info.audio_bit_rate == 128000);
    CHECK(info.sample_rate == 48000);
    CHECK(info.channels == 2);
    CHECK(info.audio_stream_index == 1);
    CHECK(info.audio_timebase.num == 1);
    CHECK(info.audio_timebase.den == 48000);

    CHECK(info.file_size == std::stoll(
        json.find("9007199254740993") != std::string::npos ? "9007199254740993" : "1024000"));
    CHECK(info.video_length == std::stoll(
        json.find("9007199254740993") != std::string::npos ? "9007199254740993" : "300"));
}

std::string validChunkMetadata(
    const std::string& fileSize,
    const std::string& videoLength)
{
    return std::string(R"({
        "has_video": true,
        "has_audio": true,
        "duration": 10.0,
        "file_size": ")") + fileSize + R"(",
        "height": 720,
        "width": 1280,
        "pixel_format": 0,
        "fps": {"num": 30, "den": 1},
        "video_bit_rate": 2500000,
        "pixel_ratio": {"num": 1, "den": 1},
        "display_ratio": {"num": 16, "den": 9},
        "vcodec": "vp9",
        "video_length": ")" + videoLength + R"(",
        "video_stream_index": 0,
        "video_timebase": {"num": 1, "den": 30},
        "interlaced_frame": false,
        "top_field_first": false,
        "acodec": "opus",
        "audio_bit_rate": 128000,
        "sample_rate": 48000,
        "channels": 2,
        "audio_stream_index": 1,
        "audio_timebase": {"num": 1, "den": 48000}
    })";
}

} // namespace

TEST_CASE("ChunkReader accepts valid metadata",
          "[chunkreader][json]")
{
    checkValidMetadata(validChunkMetadata("1024000", "300"));
}

TEST_CASE("ChunkReader accepts large string-encoded integers",
          "[chunkreader][json]")
{
    checkValidMetadata(
        validChunkMetadata("9007199254740993",
                           "9007199254740993"));
}

TEST_CASE("ChunkReader handles string-valued width",
          "[chunkreader][json][types]")
{
    const std::string metadata =
        validChunkMetadata("1024000", "300");

    std::string modified = metadata;
    const std::string original = "\"width\": 1280";
    const std::string replacement = "\"width\": \"1280\"";

    const auto position = modified.find(original);
    REQUIRE(position != std::string::npos);
    modified.replace(position, original.size(), replacement);

    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const std::string filename =
        directory.path().toStdString() + "/info.json";

    std::ofstream output(filename);
    REQUIRE(output.is_open());
    output << modified;
    output.close();

    // Record the actual Qt conversion behavior.
    REQUIRE_THROWS_AS(
        openshot::ChunkReader(
            directory.path().toStdString(),
            openshot::PREVIEW),
        openshot::InvalidJSON);
}

TEST_CASE("ChunkReader handles out-of-range width",
          "[chunkreader][json][types]")
{
    std::string modified =
        validChunkMetadata("1024000", "300");

    const std::string original = "\"width\": 1280";
    const std::string replacement =
        "\"width\": 9223372036854775807";

    const auto position = modified.find(original);
    REQUIRE(position != std::string::npos);
    modified.replace(position, original.size(), replacement);

    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const std::string filename =
        directory.path().toStdString() + "/info.json";

    std::ofstream output(filename);
    REQUIRE(output.is_open());
    output << modified;
    output.close();

    // Qt's integer conversion may return a default value
    // when the number cannot be represented as int.
    REQUIRE_THROWS_AS(
        openshot::ChunkReader(
            directory.path().toStdString(),
            openshot::PREVIEW),
        openshot::InvalidJSON);
}

namespace {

std::string replaceMetadataValue(const std::string& key,
                                 const std::string& originalValue,
                                 const std::string& replacementValue)
{
    std::string metadata = validChunkMetadata("1024000", "300");
    const std::string before = "\"" + key + "\": " + originalValue;
    const std::string after = "\"" + key + "\": " + replacementValue;
    const auto position = metadata.find(before);
    REQUIRE(position != std::string::npos);
    metadata.replace(position, before.size(), after);
    return metadata;
}

void checkMetadataFields(const std::string& metadata,
                         int expectedWidth,
                         unsigned int expectedBitrate,
                         bool expectedHasVideo)
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const std::string filename =
        directory.path().toStdString() + "/info.json";
    std::ofstream output(filename);
    REQUIRE(output.is_open());
    output << metadata;
    output.close();
    openshot::ChunkReader reader(directory.path().toStdString(),
                                openshot::PREVIEW);
    CHECK(reader.info.width == expectedWidth);
    CHECK(reader.info.video_bit_rate == expectedBitrate);
    CHECK(reader.info.has_video == expectedHasVideo);
}

} // namespace

TEST_CASE("ChunkReader preserves JsonCpp fractional integer conversion",
          "[chunkreader][json][conversion]")
{
    checkMetadataFields(replaceMetadataValue("width", "1280", "42.75"),
                        42, 2500000U, true);
}

TEST_CASE("ChunkReader preserves JsonCpp boolean and null integer conversion",
          "[chunkreader][json][conversion]")
{
    checkMetadataFields(replaceMetadataValue("width", "1280", "true"),
                        1, 2500000U, true);
    checkMetadataFields(replaceMetadataValue("width", "1280", "null"),
                        0, 2500000U, true);
}

TEST_CASE("ChunkReader preserves JsonCpp numeric boolean conversion",
          "[chunkreader][json][conversion]")
{
    checkMetadataFields(replaceMetadataValue("has_video", "true", "42"),
                        1280, 2500000U, true);
    checkMetadataFields(replaceMetadataValue("has_video", "true", "0"),
                        1280, 2500000U, false);
}

TEST_CASE("ChunkReader preserves JsonCpp unsigned conversion boundaries",
          "[chunkreader][json][conversion]")
{
    checkMetadataFields(replaceMetadataValue("video_bit_rate", "2500000",
                                             "4294967295"),
                        1280, 4294967295U, true);
    checkMetadataFields(replaceMetadataValue("video_bit_rate", "2500000",
                                             "42.75"),
                        1280, 42U, true);
    checkInvalidMetadata(replaceMetadataValue("video_bit_rate", "2500000",
                                              "-1"));
    checkInvalidMetadata(replaceMetadataValue("video_bit_rate", "2500000",
                                              "4294967296"));
    checkInvalidMetadata(replaceMetadataValue("video_bit_rate", "2500000",
                                              "\"42\""));
}
