#!/usr/bin/env python3

"""
 @file
 @brief Python source file for QtHtmlReader example
 @author Jonathan Thomas <jonathan@openshot.org>
 @author FeRD (Frank Dana) <ferdnyc@gmail.com>

 @ref License
"""

# Copyright (c) 2008-2019 OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import sys
from PyQt5.QtCore import QTimer
from PyQt5.QtGui import QGuiApplication
import openshot

app = QGuiApplication(sys.argv)

html_code = """<p><span id="red">Check out</span> this HTML!</p>"""

css_code = """
    * {font-family:sans-serif; font-size:18pt; color:#ffffff;}
    #red {color: #ff0000;}
"""

# Create a QtHtmlReader
r = openshot.QtHtmlReader(1280,      # width
                          720,       # height
                          -16,       # x offset
                          -16,       # y offset
                          openshot.GRAVITY_BOTTOM_RIGHT,
                          html_code,
                          css_code,
                          "#000000"  # background color
                          )

r.Open()  # Open the reader

r.DisplayInfo()  # Display metadata

# Set up Writer
w = openshot.FFmpegWriter("pyHtmlExample.mp4")

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

# Set a timer to terminate the app as soon as the event queue empties
QTimer.singleShot(0, app.quit)

# Run QGuiApplication to completion
sys.exit(app.exec())
