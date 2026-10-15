/**
 * @file
 * @brief Unit tests for FFmpegWriter spherical metadata
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2023 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "Frame.h"

using namespace openshot;

namespace {

uint32_t ReadBE32(const std::vector<uint8_t>& data, size_t offset)
{
    if (offset > data.size() || data.size() - offset < 4)
        throw std::runtime_error("Truncated MP4 integer");

    return (uint32_t(data[offset]) << 24)
         | (uint32_t(data[offset + 1]) << 16)
         | (uint32_t(data[offset + 2]) << 8)
         | uint32_t(data[offset + 3]);
}

uint64_t ReadBE64(const std::vector<uint8_t>& data, size_t offset)
{
    return (uint64_t(ReadBE32(data, offset)) << 32)
         | ReadBE32(data, offset + 4);
}

struct Mp4Box {
    size_t payload;
    size_t end;
    std::string type;
};

std::vector<Mp4Box> ParseBoxes(
    const std::vector<uint8_t>& data,
    size_t begin,
    size_t end)
{
    std::vector<Mp4Box> boxes;

    if (begin > end || end > data.size())
        throw std::runtime_error("Invalid MP4 box range");

    while (begin < end) {
        if (end - begin < 8)
            throw std::runtime_error("Truncated MP4 box header");

        uint64_t size = ReadBE32(data, begin);
        size_t header = 8;

        if (size == 1) {
            if (end - begin < 16)
                throw std::runtime_error("Truncated extended MP4 box");
            size = ReadBE64(data, begin + 8);
            header = 16;
        } else if (size == 0) {
            size = end - begin;
        }

        if (size < header || size > end - begin)
            throw std::runtime_error("Invalid MP4 box size");

        std::string type(
            reinterpret_cast<const char*>(data.data() + begin + 4), 4);

        boxes.push_back({
            begin + header,
            begin + static_cast<size_t>(size),
            type
        });

        begin += static_cast<size_t>(size);
    }

    return boxes;
}

bool FindPrhd(
    const std::vector<uint8_t>& data,
    size_t begin,
    size_t end,
    float& yaw,
    float& pitch,
    float& roll)
{
    for (const auto& box : ParseBoxes(data, begin, end)) {
        if (box.type == "prhd") {
            // FullBox version/flags (4 bytes), then three signed 16.16 values.
            if (box.end - box.payload < 16)
                throw std::runtime_error("Truncated prhd box");

            auto angle = [&](size_t offset) {
                const uint32_t raw = ReadBE32(data, offset);
                const int64_t signed_value =
                    (raw & 0x80000000u)
                        ? static_cast<int64_t>(raw) - 0x100000000LL
                        : static_cast<int64_t>(raw);
                return static_cast<float>(signed_value) / 65536.0f;
            };

            yaw   = angle(box.payload + 4);
            pitch = angle(box.payload + 8);
            roll  = angle(box.payload + 12);
            return true;
        }

        if (box.type == "sv3d" || box.type == "proj") {
            if (FindPrhd(data, box.payload, box.end,
                         yaw, pitch, roll))
                return true;
        }
    }

    return false;
}

bool ReadSphericalOrientation(
    const std::string& filename,
    float& yaw,
    float& pitch,
    float& roll)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    const auto length = file.tellg();
    if (length <= 0)
        return false;

    std::vector<uint8_t> data(static_cast<size_t>(length));
    file.seekg(0);

    if (!file.read(reinterpret_cast<char*>(data.data()), length))
        return false;

    // Traverse MP4 boxes by their declared boundaries.
    // Do not scan arbitrary media payloads for "sv3d".
    for (const auto& moov : ParseBoxes(data, 0, data.size())) {
        if (moov.type != "moov")
            continue;

        for (const auto& trak :
             ParseBoxes(data, moov.payload, moov.end)) {
            if (trak.type != "trak")
                continue;

            for (const auto& mdia :
                 ParseBoxes(data, trak.payload, trak.end)) {
                if (mdia.type != "mdia")
                    continue;

                for (const auto& minf :
                     ParseBoxes(data, mdia.payload, mdia.end)) {
                    if (minf.type != "minf")
                        continue;

                    for (const auto& stbl :
                         ParseBoxes(data, minf.payload, minf.end)) {
                        if (stbl.type != "stbl")
                            continue;

                        for (const auto& stsd :
                             ParseBoxes(data, stbl.payload, stbl.end)) {
                            if (stsd.type != "stsd")
                                continue;

                            // stsd FullBox header:
                            // version/flags + entry_count = 8 bytes.
                            if (stsd.end - stsd.payload < 8)
                                throw std::runtime_error("Truncated stsd");

                            const uint32_t count =
                                ReadBE32(data, stsd.payload + 4);

                            auto entries = ParseBoxes(
                                data, stsd.payload + 8, stsd.end);

                            if (entries.size() != count)
                                throw std::runtime_error(
                                    "Invalid stsd entry count");

                            for (const auto& entry : entries) {
                                if (entry.type != "avc1" &&
                                    entry.type != "avc3" &&
                                    entry.type != "hvc1" &&
                                    entry.type != "hev1")
                                    continue;

                                // ISO visual sample entry header is
                                // 78 bytes after the box header.
                                if (entry.end - entry.payload < 78)
                                    throw std::runtime_error(
                                        "Truncated visual sample entry");

                                for (const auto& child : ParseBoxes(
                                         data, entry.payload + 78,
                                         entry.end)) {
                                    if (child.type == "sv3d") {
                                        if (FindPrhd(
                                                data,
                                                child.payload,
                                                child.end,
                                                yaw, pitch, roll))
                                            return true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}

} // namespace

TEST_CASE("SphericalMetadata_WriterRegression",
          "[libopenshot][ffmpegwriter][spherical-writer]")
{
    struct Case {
        const char* filename;
        float yaw;
        float pitch;
        float roll;
    };

    const Case cases[] = {
        {"spherical_writer_regression_1.mp4", 30.0f, 0.0f, 0.0f},
        {"spherical_writer_regression_2.mp4", 45.0f, 30.0f, 15.0f},
        {"spherical_writer_regression_3.mp4", -90.0f, -30.0f, -15.0f}
    };

    for (const auto& item : cases) {
        INFO("Writer regression file: " << item.filename);

        struct TemporaryFile {
            std::string path;

            explicit TemporaryFile(const std::string& filename)
                : path(filename) {}

            ~TemporaryFile() {
                std::remove(path.c_str());
            }

            TemporaryFile(const TemporaryFile&) = delete;
            TemporaryFile& operator=(const TemporaryFile&) = delete;
        };

        TemporaryFile cleanup(item.filename);

        // Read source frames using the existing test fixture.
        FFmpegReader reader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
        reader.Open();

        FFmpegWriter writer(item.filename);

        writer.SetVideoOptions(
            true, "libx264",
            reader.info.fps,
            reader.info.width,
            reader.info.height,
            reader.info.pixel_ratio,
            false, false, 3000000);

        writer.PrepareStreams();

        writer.AddSphericalMetadata(
            "equirectangular",
            item.yaw,
            item.pitch,
            item.roll);

        writer.Open();

        for (int frame = 1; frame <= 30; ++frame) {
            std::shared_ptr<Frame> f = reader.GetFrame(frame);
            writer.WriteFrame(f);
        }

        writer.Close();
        reader.Close();

        float yaw = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;

        REQUIRE(ReadSphericalOrientation(
            item.filename, yaw, pitch, roll));

        CHECK(yaw == Approx(item.yaw).margin(0.01f));
        CHECK(pitch == Approx(item.pitch).margin(0.01f));
        CHECK(roll == Approx(item.roll).margin(0.01f));

    }
}

TEST_CASE( "SphericalMetadata_Test", "[libopenshot][ffmpegwriter]" )
{
    // Create a reader to grab some frames
    FFmpegReader r(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
    r.Open();

    // Create a spherical metadata test video
    std::string test_file = "spherical_test.mp4";

    // Create a writer
    FFmpegWriter w(test_file);
    
    // Set options - Using MP4 with H.264 for best compatibility with spherical metadata
    w.SetVideoOptions(true, "libx264", r.info.fps, r.info.width, r.info.height, 
                      r.info.pixel_ratio, false, false, 3000000);
    w.SetAudioOptions(true, "aac", r.info.sample_rate, r.info.channels, 
                      r.info.channel_layout, 128000);

    w.PrepareStreams();

    // Add spherical metadata BEFORE opening the writer
    float test_yaw = 30.0f;
    w.AddSphericalMetadata("equirectangular", test_yaw, 0.0f, 0.0f);

    // Open writer
    w.Open();

    // Write a few frames
    for (int frame = 1; frame <= 30; frame++) {
        // Get the frame
        std::shared_ptr<Frame> f = r.GetFrame(frame);
        
        // Write the frame
        w.WriteFrame(f);
    }
    
    // Close the writer & reader
    w.Close();
    r.Close();

    // Reopen the file with FFmpegReader to verify metadata was added
    FFmpegReader test_reader(test_file);
    test_reader.Open();
    
    // Display format information for debugging
    INFO("Container format: " << test_reader.info.vcodec);
    INFO("Duration: " << test_reader.info.duration);
    INFO("Width x Height: " << test_reader.info.width << "x" << test_reader.info.height);
    
    // Check metadata map contents for debugging
    INFO("Metadata entries in reader:");
    for (const auto& entry : test_reader.info.metadata) {
        INFO("  " << entry.first << " = " << entry.second);
    }
    
    // Verify presence of spherical metadata and orientation keys
    CHECK(test_reader.info.metadata.count("spherical") > 0);
    CHECK(test_reader.info.metadata["spherical"] == "1");
    CHECK(test_reader.info.metadata.count("spherical_projection") > 0);
    CHECK(test_reader.info.metadata.count("spherical_yaw")   > 0);
    CHECK(test_reader.info.metadata.count("spherical_pitch") > 0);
    CHECK(test_reader.info.metadata.count("spherical_roll")  > 0);

    // Spot-check yaw value
    float yaw_found = std::stof(test_reader.info.metadata["spherical_yaw"]);
    CHECK(yaw_found == Approx(test_yaw).margin(0.5f));

    // Clean up
    test_reader.Close();
    std::remove(test_file.c_str());
}

TEST_CASE( "SphericalMetadata_FullOrientation", "[libopenshot][ffmpegwriter]" )
{
    // Create a reader to grab some frames
    FFmpegReader r(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
    r.Open();

    // Create a spherical metadata test video
    std::string test_file = "spherical_orientation_test.mp4";

    // Create a writer
    FFmpegWriter w(test_file);
    
    // Set options - Using MP4 with H.264 for best compatibility with spherical metadata
    w.SetVideoOptions(true, "libx264", r.info.fps, r.info.width, r.info.height, 
                      r.info.pixel_ratio, false, false, 3000000);
    w.SetAudioOptions(true, "aac", r.info.sample_rate, r.info.channels, 
                      r.info.channel_layout, 128000);

    w.PrepareStreams();

    // Add spherical metadata BEFORE opening the writer
    float test_yaw = 45.0f;
    float test_pitch = 30.0f;
    float test_roll = 15.0f;
    w.AddSphericalMetadata("equirectangular", test_yaw, test_pitch, test_roll);

    // Open writer
    w.Open();

    // Write a few frames
    for (int frame = 1; frame <= 30; frame++) {
        // Get the frame
        std::shared_ptr<Frame> f = r.GetFrame(frame);
        
        // Write the frame
        w.WriteFrame(f);
    }
    
    // Close the writer & reader
    w.Close();
    r.Close();

    // Reopen the file with FFmpegReader to verify metadata was added
    FFmpegReader test_reader(test_file);
    test_reader.Open();
    
    // Check metadata map contents for debugging
    INFO("Metadata entries in reader:");
    for (const auto& entry : test_reader.info.metadata) {
        INFO("  " << entry.first << " = " << entry.second);
    }

    // Verify presence of spherical metadata and orientation keys
    CHECK(test_reader.info.metadata.count("spherical") > 0);
    CHECK(test_reader.info.metadata["spherical"] == "1");
    CHECK(test_reader.info.metadata.count("spherical_projection") > 0);
    CHECK(test_reader.info.metadata.count("spherical_yaw")   > 0);
    CHECK(test_reader.info.metadata.count("spherical_pitch") > 0);
    CHECK(test_reader.info.metadata.count("spherical_roll")  > 0);

    // Validate each orientation value
    float yaw_found   = std::stof(test_reader.info.metadata["spherical_yaw"]);
    float pitch_found = std::stof(test_reader.info.metadata["spherical_pitch"]);
    float roll_found  = std::stof(test_reader.info.metadata["spherical_roll"]);
    CHECK(yaw_found   == Approx(test_yaw).margin(0.5f));
    CHECK(pitch_found == Approx(test_pitch).margin(0.5f));
    CHECK(roll_found  == Approx(test_roll).margin(0.5f));

    // Clean up
    test_reader.Close();
    std::remove(test_file.c_str());
}