#!/usr/bin/env python3

"""
 @file
 @brief Python source file for openshot.py example
 @author Jonathan Thomas <jonathan@openshot.org>
 @author FeRD (Frank Dana) <ferdnyc@gmail.com>

 @ref License
"""

# Copyright (c) 2008-2019 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# This can be run against an uninstalled build of libopenshot, just set the
# environment variable PYTHONPATH to the location of the Python bindings.
#
# For example:
# $ PYTHONPATH=../../build/src/bindings/python python3 Example.py
#
import openshot


# Create an FFmpegReader
r = openshot.FFmpegReader("sintel_trailer-720p.mp4")

r.Open()         # Open the reader
r.DisplayInfo()  # Display metadata

# Set up Writer
w = openshot.FFmpegWriter("pythonExample.mp4")

w.SetAudioOptions(True, "libmp3lame", r.info.sample_rate, r.info.channels, r.info.channel_layout, 128000)
w.SetVideoOptions(True, "libx264", openshot.Fraction(30000, 1000), 1280, 720,
                  openshot.Fraction(1, 1), False, False, 3000000)

w.info.metadata["title"] = "testtest"
w.info.metadata["artist"] = "aaa"
w.info.metadata["album"] = "bbb"
w.info.metadata["year"] = "2015"
w.info.metadata["description"] = "ddd"
w.info.metadata["comment"] = "eee"
w.info.metadata["comment"] = "comment"
w.info.metadata["copyright"] = "copyright OpenShot!"

# Open the Writer
w.Open()

# Grab 30 frames from Reader and encode to Writer
for frame in range(100):
    f = r.GetFrame(frame)
    w.WriteFrame(f)

# Close out Reader & Writer
w.Close()
r.Close()

print("Completed successfully!")
