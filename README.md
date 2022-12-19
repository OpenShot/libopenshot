<!--
© OpenShot Studios, LLC

SPDX-License-Identifier: LGPL-3.0-or-later
-->

OpenShot Video Library (libopenshot) is a free, open-source C++ library
dedicated to delivering high quality video editing, animation, and playback
solutions to the world.

## Build Status

[![libopenshot CI Build](https://github.com/OpenShot/libopenshot/actions/workflows/ci.yml/badge.svg)](https://github.com/OpenShot/libopenshot/actions/workflows/ci.yml) [![libopenshot-audio CI Build](https://github.com/OpenShot/libopenshot-audio/actions/workflows/ci.yml/badge.svg)](https://github.com/OpenShot/libopenshot-audio/actions/workflows/ci.yml)

## Features

* Cross-Platform (Linux, Mac, and Windows)
* Multi-Layer Compositing
* Video and Audio Effects (Chroma Key, Color Adjustment, Grayscale, etc…)
* Animation Curves (Bézier, Linear, Constant)
* Time Mapping (Curve-based Slow Down, Speed Up, Reverse)
* Audio Mixing & Resampling (Curve-based)
* Audio Plug-ins (VST & AU)
* Audio Drivers (ASIO, WASAPI, DirectSound, CoreAudio, iPhone Audio,
  ALSA, JACK, and Android)
* Telecine and Inverse Telecine (Film to TV, TV to Film)
* Frame Rate Conversions
* Multi-Processor Support (Performance)
* Python and Ruby Bindings (All Features Supported)
* Qt Video Player Included (Ability to display video on any QWidget)
* Unit Tests (Stability)
* All FFmpeg Formats and Codecs Supported (Images, Videos, and Audio files)
* Full Documentation with Examples (Doxygen Generated)

## Install

Detailed instructions for building libopenshot and libopenshot-audio for
each OS. These instructions are also available in the `/docs/` source folder.

   * [Linux](https://github.com/OpenShot/libopenshot/wiki/Linux-Build-Instructions)
   * [Mac](https://github.com/OpenShot/libopenshot/wiki/Mac-Build-Instructions)
   * [Windows](https://github.com/OpenShot/libopenshot/wiki/Windows-Build-Instructions)

## Hardware Acceleration

OpenShot now supports experimental hardware acceleration, both for encoding
and decoding videos. When enabled, this can either speed up those operations
or slow them down, depending on the power and features supported by your
graphics card.

Please see [`doc/HW-ACCEL.md`](doc/HW-ACCEL.md) for more information.

## Documentation

Beautiful HTML documentation can be generated using Doxygen.
```
make doc
```
(Also available online: http://openshot.org/files/libopenshot/)

## Developers

Are you interested in becoming more involved in the development of OpenShot?
Build exciting new features, fix bugs, make friends, and become a hero!
Please read the [step-by-step](https://github.com/OpenShot/openshot-qt/wiki/Become-a-Developer)
instructions for getting source code, configuring dependencies, and building
OpenShot.

## Report a bug

You can report a new libopenshot issue directly on GitHub:

https://github.com/OpenShot/libopenshot/issues

## Websites

- https://www.openshot.org/  (Official website and blog)
- https://github.com/OpenShot/libopenshot/ (source code and issue tracker)
- https://github.com/OpenShot/libopenshot-audio/ (source code for audio library)
- https://github.com/OpenShot/openshot-qt/ (source code for Qt client)
- https://launchpad.net/openshot/

### Copyright & License

Copyright (c) 2008-2022 OpenShot Studios, LLC. This file is part of
OpenShot Video Editor (https://www.openshot.org), an open-source project
dedicated to delivering high quality video editing and animation solutions
to the world.

OpenShot Library (libopenshot) is free software: you can redistribute it
and/or modify it under the terms of the GNU Lesser General Public License
as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

OpenShot Library (libopenshot) is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with OpenShot Library. If not, see http://www.gnu.org/licenses/.

To release a commercial product which uses libopenshot (i.e. video
editing and playback), commercial licenses are also available: contact
sales@openshot.org for more information.
