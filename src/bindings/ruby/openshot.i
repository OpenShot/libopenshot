/* ####################### src/openshot.i (libopenshot) ########################
# @brief SWIG configuration for libopenshot (to generate Ruby SWIG bindings)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2019 OpenShot Studios, LLC
# <http://www.openshotstudios.com/>. This file is part of
# OpenShot Library (libopenshot), an open-source project dedicated to
# delivering high quality video editing and animation solutions to the
# world. For more information visit <http://www.openshot.org/>.
#
# OpenShot Library (libopenshot) is free software: you can redistribute it
# and/or modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# OpenShot Library (libopenshot) is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
################################################################################ */


%module openshot

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

/* Unhandled STL Exception Handling */
%include <std_except.i>

namespace std {
  template<class T> class shared_ptr {
  public:
    T *operator->();
  };
}

/* Mark these classes as shared_ptr classes */
#ifdef USE_IMAGEMAGICK
	%template(SPtrImage) std::shared_ptr<Magick::Image>;
#endif
%template(SPtrAudioBuffer) std::shared_ptr<juce::AudioSampleBuffer>;
%template(SPtrOpenFrame) std::shared_ptr<openshot::Frame>;

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
#include "Timeline.h"
#include "ZmqLogger.h"
#include "AudioDeviceInfo.h"

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

#ifdef USE_BLACKMAGIC
	%{
		#include "DecklinkReader.h"
		#include "DecklinkWriter.h"
	%}
#endif

#ifdef USE_IMAGEMAGICK
	%{
		#include "ImageReader.h"
		#include "ImageWriter.h"
		#include "TextReader.h"
	%}
#endif

%include "OpenShotVersion.h"
%include "ReaderBase.h"
%include "WriterBase.h"
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
#ifdef USE_BLACKMAGIC
	%include "DecklinkReader.h"
	%include "DecklinkWriter.h"
#endif
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
%include "Timeline.h"
%include "ZmqLogger.h"
%include "AudioDeviceInfo.h"

#ifdef USE_IMAGEMAGICK
	%include "ImageReader.h"
	%include "ImageWriter.h"
	%include "TextReader.h"
#endif


/* Effects */
%include "effects/Bars.h"
%include "effects/Blur.h"
%include "effects/Brightness.h"
%include "effects/ChromaKey.h"
%include "effects/ColorShift.h"
%include "effects/Crop.h"
%include "effects/Deinterlace.h"
%include "effects/Hue.h"
%include "effects/Mask.h"
%include "effects/Negate.h"
%include "effects/Pixelate.h"
%include "effects/Saturation.h"
%include "effects/Shift.h"
%include "effects/Wave.h"


/* Wrap std templates (list, vector, etc...) */
%template(ClipList) std::list<openshot::Clip *>;
%template(EffectBaseList) std::list<openshot::EffectBase *>;
%template(CoordinateVector) std::vector<openshot::Coordinate>;
%template(PointsVector) std::vector<openshot::Point>;
%template(FieldVector) std::vector<openshot::Field>;
%template(MappedFrameVector) std::vector<openshot::MappedFrame>;
%template(MappedMetadata) std::map<std::string, std::string>;
%template(AudioDeviceInfoVector) std::vector<openshot::AudioDeviceInfo>;
