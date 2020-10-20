/**
 * @file
 * @brief Source file for QtHtmlReader Example (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
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
#include <QGuiApplication>
#include <QTimer>

#include "OpenShot.h"
#include "CrashHandler.h"

using namespace openshot;

int main(int argc, char* argv[]) {

    QGuiApplication app(argc, argv);

    std::string html_code = R"html(<p><span id="red">Check out</span> this HTML!</p>)html";

    std::string css_code = R"css(
        * {font-family:sans-serif; font-size:18pt; color:#ffffff;}
        #red {color: #ff0000;}
    )css";

// Create a reader to generate an openshot::Frame containing text
QtHtmlReader r(1280, // width
               720, // height
               -16, // x_offset
               -16, // y_offset
               GRAVITY_BOTTOM_RIGHT, // gravity
               html_code, // html
               css_code, // css
               "#000000" // background_color
               );

    r.Open(); // Open the reader

    r.DisplayInfo();

    /* WRITER ---------------- */
    FFmpegWriter w("cppHtmlExample.mp4");

    // Set options
    //w.SetAudioOptions(true, "libmp3lame", r.info.sample_rate, r.info.channels, r.info.channel_layout, 128000);
    w.SetVideoOptions(true, "libx264", Fraction(30000, 1000), 1280, 720, Fraction(1, 1), false, false, 3000000);

    w.info.metadata["title"] = "testtest";
    w.info.metadata["artist"] = "aaa";
    w.info.metadata["album"] = "bbb";
    w.info.metadata["year"] = "2015";
    w.info.metadata["description"] = "ddd";
    w.info.metadata["comment"] = "eee";
    w.info.metadata["comment"] = "comment";
    w.info.metadata["copyright"] = "copyright OpenShot!";

    // Open writer
    w.Open();

    for (long int frame = 1; frame <= 100; ++frame)
    {
        std::shared_ptr<Frame> f = r.GetFrame(frame); // Same frame every time
        w.WriteFrame(f);
    }

    // Close writer & reader
    w.Close();
    r.Close();

    // Set a timer with 0 timeout to terminate immediately after
    // processing events
    QTimer::singleShot(0, &app, SLOT(quit()));

    // Run QGuiApplication to completion
    return app.exec();
}
