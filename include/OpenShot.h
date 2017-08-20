#ifndef OPENSHOT_H
#define OPENSHOT_H

/**
 * @file
 * @brief This header includes all commonly used headers for libopenshot, for ease-of-use.
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @mainpage OpenShot Video Editing Library C++ API
 *
 * Welcome to the OpenShot Video Editing Library (libopenshot) C++ API. This library was developed to
 * make high-quality video editing and animation solutions freely available to the world. With a focus
 * on stability, performance, and ease-of-use, we believe libopenshot is the best cross-platform,
 * open-source video editing library in the world. This library powers
 * <a href="http://www.openshot.org">OpenShot Video Editor</a> (version 2.0+), the highest rated video
 * editor available on Linux (and soon Windows & Mac). It could also <b>power</b> your next video editing project!
 *
 * Our documentation is quite extensive, including descriptions and examples of almost every class, method,
 * and parameter. Getting started is easy.
 *
 * All you need is a <b>single</b> include to get started:
 * @code
 * #include "OpenShot.h"
 * @endcode
 *
 * ### The Basics ###
 * To understand libopenshot, we must first learn about the basic building blocks:.
 *  - <b>Readers</b> - A reader is used to read a video, audio, image file, or stream and return openshot::Frame objects.
 *    - A few common readers are openshot::FFmpegReader, openshot::TextReader, openshot::ImageReader, openshot::ChunkReader, and openshot::FrameMapper
 *
 *  - <b>Writers</b> - A writer consumes openshot::Frame objects, and is used to write / create a video, audio, image file, or stream.
 *    - A few common writers are openshot::FFmpegWriter, openshot::ImageWriter, and openshot::ChunkWriter
 *
 *  - <b>Timeline</b> - A timeline allows many openshot::Clip objects to be trimmed, arranged, and layered together.
 *    - The openshot::Timeline is a special kind of reader, built from openshot::Clip objects (each clip containing a reader)
 *
 *  - <b> Keyframe</b> - A Keyframe is used to change values of properties over time on the timeline (curve-based animation).
 *    - The openshot::Keyframe, openshot::Point, and openshot::Coordinate are used to animate properties on the timeline.
 *
 * ### Example Code ###
 * Now that you understand the basic building blocks of libopenshot, lets take a look at a simple example,
 * where we use a reader to access frames of a video file.

 * @code
 * // Create a reader for a video
 * FFmpegReader r("MyAwesomeVideo.webm");
 * r.Open(); // Open the reader
 *
 * // Get frame number 1 from the video
 * std::shared_ptr<Frame> f = r.GetFrame(1);
 *
 * // Now that we have an openshot::Frame object, lets have some fun!
 * f->Display(); // Display the frame on the screen
 * f->DisplayWaveform(); // Display the audio waveform as an image
 * f->Play(); // Play the audio through your speaker
 *
 * // Close the reader
 * r.Close();
 * @endcode
 *
 * ### A Closer Look at the Timeline ###
 * The <b>following graphic</b> displays a timeline, and how clips can be arranged, scaled, and layered together. It
 * also demonstrates how the viewport can be scaled smaller than the canvas, which can be used to zoom and pan around the
 * canvas (i.e. pan & scan).
 * \image html /doc/images/Timeline_Layers.png
 *
 * ### Build Instructions (Linux, Mac, and Windows) ###
 * For a step-by-step guide to building / compiling libopenshot, check out the
 * <a href="InstallationGuide.pdf" target="_blank">Official Installation Guide</a>.
 *
 * ### Want to Learn More? ###
 * To continue learning about libopenshot, take a look at the <a href="annotated.html">full list of classes</a> available.
 *
 * ### License & Copyright ###
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

// Include the version number of OpenShot Library
#include "Version.h"

// Include all other classes
#include "AudioBufferSource.h"
#include "AudioReaderSource.h"
#include "AudioResampler.h"
#include "CacheDisk.h"
#include "CacheMemory.h"
#include "ChunkReader.h"
#include "ChunkWriter.h"
#include "Clip.h"
#include "ClipBase.h"
#include "Coordinate.h"
#ifdef USE_BLACKMAGIC
	#include "DecklinkReader.h"
	#include "DecklinkWriter.h"
#endif
#include "DummyReader.h"
#include "EffectBase.h"
#include "Effects.h"
#include "EffectInfo.h"
#include "Enums.h"
#include "Exceptions.h"
#include "ReaderBase.h"
#include "WriterBase.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "Frame.h"
#include "FrameMapper.h"
#ifdef USE_IMAGEMAGICK
	#include "ImageReader.h"
	#include "ImageWriter.h"
	#include "TextReader.h"
#endif
#include "KeyFrame.h"
#include "PlayerBase.h"
#include "Point.h"
#include "Profiles.h"
#include "QtImageReader.h"
#include "Timeline.h"

#endif
