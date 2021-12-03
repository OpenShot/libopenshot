/**
 * @file
 * @brief Source file for Example Executable (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <iostream>
#include <memory>
#include "Clip.h"
#include "Frame.h"
#include "FFmpegReader.h"
#include "Timeline.h"

using namespace openshot;


int main(int argc, char* argv[]) {

    // Types for storing time durations in whole and fractional milliseconds
    using ms = std::chrono::milliseconds;
    using s = std::chrono::seconds;
    using double_ms = std::chrono::duration<double, ms::period>;

    // FFmpeg Reader performance test
    const auto total_1 = std::chrono::high_resolution_clock::now();
    FFmpegReader r9("/home/jonathan/Videos/sintel_trailer-1080p.mp4");
    r9.Open();
    for (long int frame = 1; frame <= 1000; frame++)
    {
        const auto time1 = std::chrono::high_resolution_clock::now();
        std::shared_ptr<Frame> f = r9.GetFrame(frame);
        const auto time2 = std::chrono::high_resolution_clock::now();
        std::cout << "FFmpegReader: " << frame
                  << " (" << double_ms(time2 - time1).count() << " ms)\n";
    }
    const auto total_2 = std::chrono::high_resolution_clock::now();
    auto total_sec = std::chrono::duration_cast<ms>(total_2 - total_1);
    std::cout << "FFmpegReader TOTAL: " << total_sec.count() << " ms\n";
    r9.Close();


    // Timeline Reader performance test
    Timeline tm(r9.info);
    Clip *c = new Clip(&r9);
    tm.AddClip(c);
    tm.Open();

    const auto total_3 = std::chrono::high_resolution_clock::now();
    for (long int frame = 1; frame <= 1000; frame++)
    {
        const auto time1 = std::chrono::high_resolution_clock::now();
        std::shared_ptr<Frame> f = tm.GetFrame(frame);
        const auto time2 = std::chrono::high_resolution_clock::now();
        std::cout << "Timeline: " << frame
                  << " (" << double_ms(time2 - time1).count() << " ms)\n";
    }
    const auto total_4 = std::chrono::high_resolution_clock::now();
    total_sec = std::chrono::duration_cast<ms>(total_4 - total_3);
    std::cout << "Timeline TOTAL: " << total_sec.count() << " ms\n";
    tm.Close();

    std::cout << "Completed successfully!\n";

    return 0;
}
