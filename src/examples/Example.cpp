/**
 * @file
 * @brief Source file for Example Executable (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

    // Create and open reader
    FFmpegReader r("/home/jonathan/sintel-120fps-crash.mp4");
    r.Open();

    // Enable debug logging
    ZmqLogger::Instance()->Enable(false);
    ZmqLogger::Instance()->Path("/home/jonathan/.openshot_qt/libopenshot2.log");
    CrashHandler::Instance();

    // Loop a few times
    for (int attempt = 1; attempt < 10; attempt++) {
        cout << "** Attempt " << attempt << " **" << endl;

        // Read every frame in reader as fast as possible
        for (int frame_number = 1; frame_number < r.info.video_length; frame_number++) {
            // Get frame object
            std::shared_ptr<Frame> f = r.GetFrame(frame_number);

            // Print frame numbers
            cout << frame_number << " [" << f->number << "], " << flush;
            if (frame_number % 10 == 0)
                cout << endl;
        }
    }
    cout << "Completed successfully!" << endl;

    // Close reader
    r.Close();

    return 0;
}