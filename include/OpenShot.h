#ifndef OPENSHOT_H
#define OPENSHOT_H

/**
 * \file
 * \brief This header includes all commonly used headers, for ease-of-use
 * \author Copyright (c) 2011 Jonathan Thomas
 */

/**
 * \mainpage OpenShot Video Editing Library C++ API
 *
 * Welcome to the OpenShot Video Editing Library C++ API.  This library is a video editing and animation
 * framework, which powers the <a href="http://www.openshot.org">OpenShot Video Editor</a> application.
 * It can also be used to power your next C++ video editing project!  It is very powerful, supporting many
 * different framerate mapping conversions as well as many different keyframe interpolation methods, such
 * as Bezier curves, Linear, and Constant.
 *
 * All you need is a single <b>include</b> to get started:
 * \code
 * #include "OpenShot.h"
 * \endcode

 * Please read this documentation to learn all about how the OpenShot Video Editing Library works, and how
 * you can use it on your next video editing application.
 */

#include "AudioBufferSource.h"
#include "AudioResampler.h"
#include "Cache.h"
#include "Clip.h"
#include "Coordinate.h"
#ifdef USE_BLACKMAGIC
	#include "DecklinkReader.h"
	#include "DecklinkWriter.h"
#endif
#include "DummyReader.h"
#include "Exceptions.h"
#include "FileReaderBase.h"
#include "FileWriterBase.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "Frame.h"
#include "FrameMapper.h"
#include "FrameRate.h"
#include "ImageReader.h"
#include "KeyFrame.h"
#include "Player.h"
#include "Point.h"
#include "Sleep.h"
#include "TextReader.h"
#include "Timeline.h"


#endif
