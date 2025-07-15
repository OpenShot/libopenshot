/**
 * @file
 * @brief SWIG configuration for libopenshot (to generate Python SWIG bindings)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
*/
// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

%module("threads"=1) openshot

/* Suppress warnings about ignored operator= */
%warnfilter(362);

/* Don't generate multiple wrappers for functions with default args */
%feature("compactdefaultargs", "1");

/* Enable inline documentation */
%feature("autodoc", "1");

/* Include various SWIG helpers */
%include "typemaps.i"
%include "std_string.i"
%include "std_list.i"
%include "std_vector.i"
%include "std_map.i"
%include <stdint.i>

/* Unhandled STL Exception Handling */
%include <std_except.i>

/* Include shared pointer code */
%include <std_shared_ptr.i>

/* Mark these classes as shared_ptr classes */
#ifdef USE_IMAGEMAGICK
	%shared_ptr(Magick::Image)
#endif
%shared_ptr(juce::AudioBuffer<float>)
%shared_ptr(openshot::Frame)

/* Instantiate the required template specializations */
%template() std::map<std::string, int>;
%template() std::pair<int, int>;
%template() std::vector<int>;
%template() std::vector<float>;
%template() std::pair<double, double>;
%template() std::pair<float, float>;
%template() std::pair<std::string, std::string>;
%template() std::vector<std::pair<std::string, std::string>>;
%template() std::vector<std::vector<float>>;

%{
/* Ruby and FFmpeg define competing RSHIFT macros,
 * so we move Ruby's out of the way for now. We'll
 * restore it after dealing with FFmpeg's
 */
#ifdef RSHIFT
  #define RB_RSHIFT(a, b) RSHIFT(a, b)
  #undef RSHIFT
#endif
#include "OpenShotVersion.h"
#include "ReaderBase.h"
#include "WriterBase.h"
#include "AudioDevices.h"
#include "AudioWaveformer.h"
#include "CacheBase.h"
#include "CacheDisk.h"
#include "CacheMemory.h"
#include "ChannelLayouts.h"
#include "ChunkReader.h"
#include "ChunkWriter.h"
#include "ClipBase.h"
#include "Clip.h"
#include "Coordinate.h"
#include "Color.h"
#include "DummyReader.h"
#include "EffectBase.h"
#include "Effects.h"
#include "EffectInfo.h"
#include "Enums.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "Frame.h"
#include "FrameMapper.h"
#include "PlayerBase.h"
#include "Point.h"
#include "Profiles.h"
#include "QtHtmlReader.h"
#include "QtImageReader.h"
#include "QtPlayer.h"
#include "QtTextReader.h"
#include "KeyFrame.h"
#include "RendererBase.h"
#include "Settings.h"
#include "TimelineBase.h"
#include "Timeline.h"
#include "Qt/VideoCacheThread.h"
#include "ZmqLogger.h"

/* Move FFmpeg's RSHIFT to FF_RSHIFT, if present */
#ifdef RSHIFT
  #define FF_RSHIFT(a, b) RSHIFT(a, b)
  #undef RSHIFT
#endif
/* And restore Ruby's RSHIFT, if we captured it */
#ifdef RB_RSHIFT
  #define RSHIFT(a, b) RB_RSHIFT(a, b)
#endif
%}

// Prevent SWIG from ever generating a wrapper for juce::Thread’s constructor (or run())
%ignore juce::Thread::Thread;

#ifdef USE_IMAGEMAGICK
	%{
		#include "ImageReader.h"
		#include "ImageWriter.h"
		#include "TextReader.h"
	%}
#endif

/* Wrap std templates (list, vector, etc...) */
%template(ClipList) std::list<openshot::Clip *>;
%template(EffectBaseList) std::list<openshot::EffectBase *>;
%template(CoordinateVector) std::vector<openshot::Coordinate>;
%template(PointsVector) std::vector<openshot::Point>;
%template(FieldVector) std::vector<openshot::Field>;
%template(MappedFrameVector) std::vector<openshot::MappedFrame>;
%template(MetadataMap) std::map<std::string, std::string>;

/* Deprecated */
%template(AudioDeviceInfoVector) std::vector<openshot::AudioDeviceInfo>;

%include "OpenShotVersion.h"
%include "ReaderBase.h"
%include "WriterBase.h"
%include "AudioDevices.h"
%include "AudioWaveformer.h"
%include "CacheBase.h"
%include "CacheDisk.h"
%include "CacheMemory.h"
%include "ChannelLayouts.h"
%include "ChunkReader.h"
%include "ChunkWriter.h"
%include "ClipBase.h"
%include "Clip.h"
%include "Coordinate.h"
%include "Color.h"
%include "DummyReader.h"
%include "EffectBase.h"
%include "Effects.h"
%include "EffectInfo.h"
%include "Enums.h"
%include "Exceptions.h"

/* Ruby and FFmpeg define competing RSHIFT macros,
 * so we move Ruby's out of the way for now. We'll
 * restore it after dealing with FFmpeg's
 */
#ifdef RSHIFT
  #define RB_RSHIFT(a, b) RSHIFT(a, b)
  #undef RSHIFT
#endif

%include "FFmpegReader.h"
%include "FFmpegWriter.h"

/* Move FFmpeg's RSHIFT to FF_RSHIFT, if present */
#ifdef RSHIFT
  #define FF_RSHIFT(a, b) RSHIFT(a, b)
  #undef RSHIFT
#endif
/* And restore Ruby's RSHIFT, if we captured it */
#ifdef RB_RSHIFT
  #define RSHIFT(a, b) RB_RSHIFT(a, b)
#endif

%include "Fraction.h"
%include "Frame.h"
%include "FrameMapper.h"
%include "PlayerBase.h"
%include "Point.h"
%include "Profiles.h"
%include "QtHtmlReader.h"
%include "QtImageReader.h"
%include "QtPlayer.h"
%include "QtTextReader.h"
%include "KeyFrame.h"
%include "RendererBase.h"
%include "Settings.h"
%include "TimelineBase.h"
%include "Qt/VideoCacheThread.h"
%include "Timeline.h"
%include "ZmqLogger.h"

#ifdef USE_IMAGEMAGICK
	%include "ImageReader.h"
	%include "ImageWriter.h"
	%include "TextReader.h"
#endif

/* Effects */
%include "effects/Bars.h"
%include "effects/Blur.h"
%include "effects/Brightness.h"
%include "effects/Caption.h"
%include "effects/ChromaKey.h"
%include "effects/ColorShift.h"
%include "effects/Crop.h"
%include "effects/Deinterlace.h"
%include "effects/Hue.h"
%include "effects/LensFlare.h"
%include "effects/Mask.h"
%include "effects/Negate.h"
%include "effects/Pixelate.h"
%include "effects/Saturation.h"
%include "effects/Shift.h"
%include "effects/Wave.h"


