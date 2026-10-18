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

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "Frame.h"

using namespace openshot;


// Inspect the generated MP4 independently of FFmpeg's spherical metadata reader.
static bool read_orientation_box(std::ifstream& file, std::streamoff begin,
                                 std::streamoff end, std::array<double, 3>& angles)
{
    auto read_u32 = [&file]() {
        unsigned char bytes[4] = {};
        file.read(reinterpret_cast<char*>(bytes), 4);
        REQUIRE(file.good());
        return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
               (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
    };
    for (auto offset = begin; offset < end;) {
        REQUIRE(end - offset >= 8);
        file.seekg(offset);
        const auto size = read_u32();
        REQUIRE(size >= 8);
        REQUIRE(std::streamoff(size) <= end - offset);
        char type_bytes[4];
        file.read(type_bytes, 4);
        REQUIRE(file.good());
        const std::string type(type_bytes, 4);
        auto child = offset + 8;
        if (type == "prhd") {
            REQUIRE(size == 24);
            REQUIRE(read_u32() == 0); // Version and flags.
            for (auto& angle : angles) {
                const auto value = read_u32();
                const int64_t signed_value = value <= INT32_MAX
                    ? int64_t(value) : int64_t(value) - (int64_t(1) << 32);
                angle = signed_value / 65536.0;
            }
            return true;
        }
        if (type == "stsd")
            child += 8; // Version, flags, and sample-entry count.
        else if (type == "avc1" || type == "avc3")
            child += 78; // Visual sample-entry header.
        else if (type != "moov" && type != "trak" && type != "mdia" &&
                 type != "minf" && type != "stbl" && type != "sv3d" && type != "proj") {
            offset += size;
            continue;
        }
        REQUIRE(child <= offset + size);
        if (read_orientation_box(file, child, offset + size, angles))
            return true;
        offset += size;
    }
    return false;
}

static void check_written_orientation(const std::string& path,
                                      const std::array<double, 3>& expected)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    REQUIRE(file.is_open());
    const std::streamoff size = file.tellg();
    REQUIRE(size > 0);
    std::array<double, 3> actual;
    REQUIRE(read_orientation_box(file, 0, size, actual));
    for (size_t i = 0; i < actual.size(); ++i)
        REQUIRE(actual[i] == Approx(expected[i]).margin(0.5));
}

static bool has_ffmpeg_orientation_bug(float yaw, float pitch, float roll)
{
    const auto major = avformat_version() >> 16;
    // libavformat 62 and 63 correspond to FFmpeg 8 and 9.
    if ((major == 62 || major == 63) && yaw == 0 && pitch == 0 && roll == 0) {
        WARN("Skipping spherical reader angle assertions: FFmpeg 8/9 returned 0,0,0. "
             "Written MP4 angles were verified independently. Upstream patch submitted "
             "to ffmpeg-devel: avformat/mov: preserve spherical orientation when reading vexu. "
             "FFmpeg version: " << av_version_info());
        return true;
    }
    return false;
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

    check_written_orientation(test_file, {test_yaw, 0.0, 0.0});

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
    REQUIRE(test_reader.info.metadata.count("spherical") > 0);
    REQUIRE(test_reader.info.metadata["spherical"] == "1");
    REQUIRE(test_reader.info.metadata.count("spherical_projection") > 0);
    REQUIRE(test_reader.info.metadata.count("spherical_yaw")   > 0);
    REQUIRE(test_reader.info.metadata.count("spherical_pitch") > 0);
    REQUIRE(test_reader.info.metadata.count("spherical_roll")  > 0);

    // Spot-check yaw value
    float yaw_found = std::stof(test_reader.info.metadata["spherical_yaw"]);
    const float pitch_found = std::stof(test_reader.info.metadata["spherical_pitch"]);
    const float roll_found = std::stof(test_reader.info.metadata["spherical_roll"]);
    if (!has_ffmpeg_orientation_bug(yaw_found, pitch_found, roll_found)) {
        CHECK(yaw_found == Approx(test_yaw).margin(0.5f));
        CHECK(pitch_found == Approx(0.0f).margin(0.5f));
        CHECK(roll_found == Approx(0.0f).margin(0.5f));
    }

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

    check_written_orientation(test_file, {test_yaw, test_pitch, test_roll});

    // Reopen the file with FFmpegReader to verify metadata was added
    FFmpegReader test_reader(test_file);
    test_reader.Open();
    
    // Check metadata map contents for debugging
    INFO("Metadata entries in reader:");
    for (const auto& entry : test_reader.info.metadata) {
        INFO("  " << entry.first << " = " << entry.second);
    }

    // Verify presence of spherical metadata and orientation keys
    REQUIRE(test_reader.info.metadata.count("spherical") > 0);
    REQUIRE(test_reader.info.metadata["spherical"] == "1");
    REQUIRE(test_reader.info.metadata.count("spherical_projection") > 0);
    REQUIRE(test_reader.info.metadata.count("spherical_yaw")   > 0);
    REQUIRE(test_reader.info.metadata.count("spherical_pitch") > 0);
    REQUIRE(test_reader.info.metadata.count("spherical_roll")  > 0);

    // Validate each orientation value
    float yaw_found   = std::stof(test_reader.info.metadata["spherical_yaw"]);
    float pitch_found = std::stof(test_reader.info.metadata["spherical_pitch"]);
    float roll_found  = std::stof(test_reader.info.metadata["spherical_roll"]);
    if (!has_ffmpeg_orientation_bug(yaw_found, pitch_found, roll_found)) {
        CHECK(yaw_found   == Approx(test_yaw).margin(0.5f));
        CHECK(pitch_found == Approx(test_pitch).margin(0.5f));
        CHECK(roll_found  == Approx(test_roll).margin(0.5f));
    }

    // Clean up
    test_reader.Close();
    std::remove(test_file.c_str());
}