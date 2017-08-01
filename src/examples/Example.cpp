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
#include <tr1/memory>
#include "../../include/OpenShot.h"

using namespace openshot;
using namespace tr1;


int main(int argc, char* argv[]) {
    FFmpegReader rTest("/home/jonathan/Videos/sintel_trailer-720p.mp4");
    rTest.Open();

    /* WRITER ---------------- */
    FFmpegWriter w("/home/jonathan/output%05d.jpg");

    // Set options
    w.SetVideoOptions(true, "mjpeg", Fraction(24, 1), 1280, 720, Fraction(1, 1), false, false, 3000000);

//	w.SetOption(VIDEO_STREAM, "qmin", "2" );
//	w.SetOption(VIDEO_STREAM, "qmax", "30" );
//	w.SetOption(VIDEO_STREAM, "crf", "10" );
//	w.SetOption(VIDEO_STREAM, "rc_min_rate", "2000000" );
//	w.SetOption(VIDEO_STREAM, "rc_max_rate", "4000000" );
//    w.SetOption(VIDEO_STREAM, "max_b_frames", "0" );
//    w.SetOption(VIDEO_STREAM, "b_frames", "0" );

    // Open writer
    w.Open();

    // Prepare Streams
    w.PrepareStreams();


    // Write header
    w.WriteHeader();

    // Write some frames
    w.WriteFrame(&rTest, 500, 505);

    // Close writer & reader
    w.Close();
    rTest.Close();

    return 0;
}