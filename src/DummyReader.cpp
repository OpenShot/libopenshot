#include "../include/DummyReader.h"

using namespace openshot;

// Constructor for DummyReader.  Pass a framerate and samplerate.
DummyReader::DummyReader(Framerate fps, int width, int height, int sample_rate, int channels, float duration) :
		fps(fps), width(width), height(height), sample_rate(sample_rate), channels(channels), duration(duration)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Open image file
void DummyReader::Open() throw(InvalidFile)
{
	// Open reader if not already open
	if (!is_open)
	{
		// Create or get frame object
		image_frame = tr1::shared_ptr<Frame>(new Frame(1, width, height, "#000000", sample_rate, channels));

		// Add Image data to frame
		image_frame->AddImage(new Magick::Image(Magick::Geometry(width, height), Magick::Color("#000000")));

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.file_size = width * height * sizeof(int);
		info.vcodec = "raw";
		info.width = width;
		info.height = height;
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = duration;
		info.fps.num = fps.GetFraction().num;
		info.fps.den = fps.GetFraction().den;
		info.video_timebase.num = fps.GetFraction().den;
		info.video_timebase.den = fps.GetFraction().num;
		info.video_length = round(info.duration * info.fps.ToDouble());

		// Calculate the DAR (display aspect ratio)
		Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

		// Reduce size fraction
		size.Reduce();

		// Set the ratio based on the reduced fraction
		info.display_ratio.num = size.num;
		info.display_ratio.den = size.den;

		// Mark as "open"
		is_open = true;
	}
}

// Close image file
void DummyReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Mark as "closed"
		is_open = false;
	}
}

// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> DummyReader::GetFrame(int requested_frame) throw(ReaderClosed)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The ImageReader is closed.  Call Open() before calling this method.", "dummy");

	if (image_frame)
	{
		// Always return same frame (regardless of which frame number was requested)
		image_frame->number = requested_frame;
		return image_frame;
	}
	else
		// no frame loaded
		throw InvalidFile("No frame could be created from this type of file.", "dummy");
}


