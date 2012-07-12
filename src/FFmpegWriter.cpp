#include "../include/FFmpegWriter.h"

using namespace openshot;

FFmpegWriter::FFmpegWriter(string path) throw(InvalidFile, InvalidFormat, InvalidCodec)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Initialize FFMpeg, and register all formats and codecs
	av_register_all();
}

// Set video export options
void FFmpegWriter::SetVideoOptions(string codec, Fraction fps, int width, int height, Fraction display_ratio,
		Fraction pixel_ratio, bool interlaced, bool top_field_first, int bit_rate)
{

}

// Set audio export options
void FFmpegWriter::SetAudioOptions(string codec, int sample_rate, int channels, int bit_rate)
{

}

// Set custom options (some codecs accept additional params)
void FFmpegWriter::SetOption(Stream_Type stream, string name, double value)
{

}

// Write the file header (after the options are set)
void FFmpegWriter::WriteHeader()
{

}

// Write a single frame
void FFmpegWriter::WriteFrame(Frame frame)
{

}

// Write a block of frames from a reader
void FFmpegWriter::WriteFrame(FileReaderBase* reader, int start, int length)
{

}

// Write the file trailer (after all frames are written)
void FFmpegWriter::WriteTrailer()
{

}

// Close the writer
void FFmpegWriter::Close()
{

}
