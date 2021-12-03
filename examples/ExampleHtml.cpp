/**
 * @file
 * @brief Source file for QtHtmlReader Example (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <iostream>
#include <memory>
#include <QGuiApplication>
#include <QTimer>

#include "QtHtmlReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "Enums.h"          // for GRAVITY_BOTTOM_RIGHT
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
