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

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "Frame.h"

using namespace openshot;

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