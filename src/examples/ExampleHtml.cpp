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
//#include "../../include/CrashHandler.h"
#include <QGuiApplication>
#include <QTimer>

using namespace openshot;

int main(int argc, char* argv[]) {

    QGuiApplication app(argc, argv);

    // Create a reader to generate an openshot::Frame containing text
    QtHtmlReader r(720, // width
                   480, // height
                   5, // x_offset
                   5, // y_offset
                   GRAVITY_CENTER, // gravity
                   "<span style=\"font-family:sans-serif; color:#fff;\"><b>Check out</b> this Text!</span>", // html
                   "b { color: #ff0000; }",
                   "#000000" // background_color
                   );

    r.Open(); // Open the reader

    r.DisplayInfo();

    /* WRITER ---------------- */
    FFmpegWriter w9("/var/tmp/metadata.mp4");

    // Set options
    //w9.SetAudioOptions(true, "libmp3lame", r.info.sample_rate, r9.info.channels, r9.info.channel_layout, 128000);
    w9.SetVideoOptions(true, "libx264", Fraction{30000, 1000}, 720, 480, Fraction(1,1), false, false, 3000000);

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

    for (long int frame = 1; frame <= 30; ++frame)
    {
        std::shared_ptr<Frame> f = r.GetFrame(frame); // Same frame every time
        w9.WriteFrame(f);
    }

    // Close writer & reader
    w9.Close();
    r.Close();

    // Set a timer with 0 timeout to terminate immediately after
    // processing events
    QTimer::singleShot(0, &app, SLOT(quit()));

    // Run QGuiApplication to completion
    return app.exec();
}
