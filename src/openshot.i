%module openshot

%include "typemaps.i"
%include "std_string.i"

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
#include "../include/FileReaderBase.h"
#include "../include/FileWriterBase.h"
#include "../include/Cache.h"
#include "../include/Clip.h"
#include "../include/ChunkReader.h"
#include "../include/ChunkWriter.h"
#include "../include/Coordinate.h"
#include "../include/DummyReader.h"
#include "../include/Exceptions.h"
#include "../include/FFmpegReader.h"
#include "../include/FFmpegWriter.h"
#include "../include/Fraction.h"
#include "../include/Frame.h"
#include "../include/FrameMapper.h"
#include "../include/FrameRate.h"
#include "../include/ImageReader.h"
#include "../include/Player.h"
#include "../include/Point.h"
#include "../include/KeyFrame.h"
#include "../include/TextReader.h"
#include "../include/Timeline.h"
%}

#ifdef USE_BLACKMAGIC
	%{
		#include "../include/DecklinkReader.h"
		#include "../include/DecklinkWriter.h"
	%}
#endif

%include "../include/FileReaderBase.h"
%include "../include/FileWriterBase.h"
%include "../include/Cache.h"
%include "../include/ChunkReader.h"
%include "../include/ChunkWriter.h"
%include "../include/Clip.h"
%include "../include/Coordinate.h"
#ifdef USE_BLACKMAGIC
	%include "../include/DecklinkReader.h"
	%include "../include/DecklinkWriter.h"
#endif
%include "../include/DummyReader.h"
%include "../include/Exceptions.h"
%include "../include/FFmpegReader.h"
%include "../include/FFmpegWriter.h"
%include "../include/Fraction.h"
%include "../include/Frame.h"
%include "../include/FrameMapper.h"
%include "../include/FrameRate.h"
%include "../include/ImageReader.h"
%include "../include/Player.h"
%include "../include/Point.h"
%include "../include/KeyFrame.h"
%include "../include/TextReader.h"
%include "../include/Timeline.h"