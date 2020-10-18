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
#include "../../include/OpenShot.h"
#include "../../include/CrashHandler.h"

using namespace openshot;


int main(int argc, char* argv[]) {

    Settings *s = Settings::Instance();
    s->HARDWARE_DECODER = 2; // 1 VA-API, 2 NVDEC, 6 VDPAU
    s->HW_DE_DEVICE_SET = 0;

    std::string input_filepath = TEST_MEDIA_PATH;
    input_filepath += "sintel_trailer-720p.mp4";

    FFmpegReader r9(input_filepath);
    r9.Open();
    r9.DisplayInfo();

    /* WRITER ---------------- */
    FFmpegWriter w9("metadata.mp4");

    // Set options
    w9.SetAudioOptions(true, "libmp3lame", r9.info.sample_rate, r9.info.channels, r9.info.channel_layout, 128000);
    w9.SetVideoOptions(true, "libx264", r9.info.fps, 1024, 576, Fraction(1,1), false, false, 3000000);

    w9.info.metadata["title"] = "testtest";
	w9.info.metadata["artist"] = "aaa";
	w9.info.metadata["album"] = "bbb";
	w9.info.metadata["year"] = "2015";
	w9.info.metadata["description"] = "ddd";
	w9.info.metadata["comment"] = "eee";
	w9.info.metadata["comment"] = "comment";
    w9.info.metadata["copyright"] = "copyright OpenShot!";

    // Open writer
    w9.Open();

    for (long int frame = 1; frame <= 100; frame++)
    {
        //int frame_number = (rand() % 750) + 1;
        int frame_number = frame;
        std::shared_ptr<Frame> f = r9.GetFrame(frame_number);
        w9.WriteFrame(f);
    }

    // Close writer & reader
    w9.Close();

    // Close timeline
    r9.Close();

	std::cout << "Completed successfully!" << std::endl;

    return 0;
}
