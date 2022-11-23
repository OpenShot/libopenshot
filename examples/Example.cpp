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

    // FFmpeg Reader performance test
    FFmpegReader r9("/home/jonathan/Downloads/pts-test-files/broken-files/lady-talking-1.mp4");
    r9.Open();
    for (long int frame = 1; frame <= r9.info.video_length; frame++)
    {
        std::cout << "Requesting Frame: #: " << frame << std::endl;
        std::shared_ptr<Frame> f = r9.GetFrame(frame);
    }
    r9.Close();

    exit(0);
}
