/**
 * @file
 * @brief Source file for Example Executable (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <fstream>
#include <iostream>
#include <memory>
#include "OpenShot.h"
#include "CrashHandler.h"

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
        std::cout << "FFmpegReader: " << frame << " (" << double_ms(time2 - time1).count() << " ms)" << std::endl;
    }
    const auto total_2 = std::chrono::high_resolution_clock::now();
    auto total_sec = std::chrono::duration_cast<ms>(total_2 - total_1);
    std::cout << "FFmpegReader TOTAL: " << total_sec.count() << " ms" << std::endl;
    r9.Close();


    // Timeline Reader performance test
    Timeline tm(r9.info.width, r9.info.height, r9.info.fps, r9.info.sample_rate, r9.info.channels, r9.info.channel_layout);
    Clip *c = new Clip(&r9);
    tm.AddClip(c);
    tm.Open();

    const auto total_3 = std::chrono::high_resolution_clock::now();
    for (long int frame = 1; frame <= 1000; frame++)
    {
        const auto time1 = std::chrono::high_resolution_clock::now();
        std::shared_ptr<Frame> f = tm.GetFrame(frame);
        const auto time2 = std::chrono::high_resolution_clock::now();
        std::cout << "Timeline: " << frame << " (" << double_ms(time2 - time1).count() << " ms)" << std::endl;
    }
    const auto total_4 = std::chrono::high_resolution_clock::now();
    total_sec = std::chrono::duration_cast<ms>(total_4 - total_3);
    std::cout << "Timeline TOTAL: " << total_sec.count() << " ms" << std::endl;
    tm.Close();

    std::cout << "Completed successfully!" << std::endl;

    return 0;
}
