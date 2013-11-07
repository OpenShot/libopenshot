##################### src/openshot.i (libopenshot) #######################
# @brief SWIG configuration for libopenshot (to generate Python bindings)
# @author Jonathan Thomas <jonathan@openshot.org>
#
# @section LICENSE
#
# Copyright (c) 2008-2013 OpenShot Studios, LLC
# (http://www.openshotstudios.com). This file is part of
# OpenShot Library (http://www.openshot.org), an open-source project
# dedicated to delivering high quality video editing and animation solutions
# to the world.
#
# OpenShot Library is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# OpenShot Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
################################################################################


%module openshot

%include "typemaps.i"
%include "std_string.i"
%include "std_list.i"
%include "std_vector.i"

/* Unhandled STL Exception Handling */
%include <std_except.i>

/* Include shared pointer code */
#define SWIG_SHARED_PTR_SUBNAMESPACE tr1
%include <std_shared_ptr.i>


/* Mark these classes as shared_ptr classes */
%shared_ptr(Magick::Image)
%shared_ptr(juce::AudioSampleBuffer)
%shared_ptr(openshot::Frame)
%shared_ptr(Frame)

%{
#include "../include/ReaderBase.h"
#include "../include/WriterBase.h"
#include "../include/Cache.h"
#include "../include/ChunkReader.h"
#include "../include/ChunkWriter.h"
#include "../include/ClipBase.h"
#include "../include/Clip.h"
#include "../include/Coordinate.h"
#include "../include/Color.h"
#include "../include/DummyReader.h"
#include "../include/EffectBase.h"
#include "../include/Exceptions.h"
#include "../include/FFmpegReader.h"
#include "../include/FFmpegWriter.h"
#include "../include/Fraction.h"
#include "../include/Frame.h"
#include "../include/FrameMapper.h"
#include "../include/FrameRate.h"
#include "../include/ImageReader.h"
#include "../include/PlayerBase.h"
#include "../include/Point.h"
#include "../include/KeyFrame.h"
#include "../include/SDLPlayer.h"
#include "../include/TextReader.h"
#include "../include/Timeline.h"

/* Effects */
#include "../include/effects/ChromaKey.h"
#include "../include/effects/Deinterlace.h"
#include "../include/effects/Mask.h"
#include "../include/effects/Negate.h"

%}

#ifdef USE_BLACKMAGIC
	%{
		#include "../include/DecklinkReader.h"
		#include "../include/DecklinkWriter.h"
	%}
#endif

%include "../include/ReaderBase.h"
%include "../include/WriterBase.h"
%include "../include/Cache.h"
%include "../include/ChunkReader.h"
%include "../include/ChunkWriter.h"
%include "../include/ClipBase.h"
%include "../include/Clip.h"
%include "../include/Coordinate.h"
%include "../include/Color.h"
#ifdef USE_BLACKMAGIC
	%include "../include/DecklinkReader.h"
	%include "../include/DecklinkWriter.h"
#endif
%include "../include/DummyReader.h"
%include "../include/EffectBase.h"
%include "../include/Exceptions.h"
%include "../include/FFmpegReader.h"
%include "../include/FFmpegWriter.h"
%include "../include/Fraction.h"
%include "../include/Frame.h"
%include "../include/FrameMapper.h"
%include "../include/FrameRate.h"
%include "../include/ImageReader.h"
%include "../include/PlayerBase.h"
%include "../include/Point.h"
%include "../include/KeyFrame.h"
%include "../include/SDLPlayer.h"
%include "../include/TextReader.h"
%include "../include/Timeline.h"

/* Effects */
%include "../include/effects/ChromaKey.h"
%include "../include/effects/Deinterlace.h"
%include "../include/effects/Mask.h"
%include "../include/effects/Negate.h"


/* Wrap std templates (list, vector, etc...) */
namespace std {
 %template(ClipList) list<Clip *>;
 %template(EffectBaseList) list<EffectBase *>;
 %template(CoordinateVector) vector<Coordinate>;
 %template(PointsVector) vector<Point>;
 %template(FieldVector) vector<Field>;
 %template(MappedFrameVector) vector<MappedFrame>;
 
}