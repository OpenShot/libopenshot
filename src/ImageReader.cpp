#include "../include/ImageReader.h"

using namespace openshot;

ImageReader::ImageReader(string path) : path(path), is_open(false)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Open image file
void ImageReader::Open() throw(InvalidFile)
{
	// Open reader if not already open
	if (!is_open)
	{
		// Attempt to open file
		Magick::Image* source = NULL;
		try
		{
			// load image
			source = new Magick::Image(path);
		}
		catch (Magick::Exception e) {
			// raise exception
			throw InvalidFile("File could not be opened.", path);
		}

		// Create or get frame object
		image_frame = new Frame(1, source->size().width(), source->size().height(), "#000000", 0, 2);

		// Add Image data to frame
		image_frame->AddImage(source);

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.file_size = source->fileSize();
		info.vcodec = source->format();
		info.width = source->size().width();
		info.height = source->size().height();
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = 60 * 60 * 24; // 24 hour duration
		info.fps.num = 30;
		info.fps.den = 1;
		info.video_timebase.num = 1;
		info.video_timebase.den = 30;
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
void ImageReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Deallocate frame
		delete image_frame;

		// Mark as "closed"
		is_open = false;
	}
}

// Get an openshot::Frame object for a specific frame number of this reader.
Frame* ImageReader::GetFrame(int requested_frame) throw(ReaderClosed)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The ImageReader is closed.  Call Open() before calling this method.", path);

	if (image_frame)
		// Always return same frame (regardless of which frame number was requested)
		return image_frame;
	else
		// no frame loaded
		throw InvalidFile("No frame could be created from this type of file.", path);
}


