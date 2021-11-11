/**
 * @file
 * @brief Source file for ImageWriter class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC, Fabrice Bellard
//
// SPDX-License-Identifier: LGPL-3.0-or-later

//Require ImageMagick support
#ifdef USE_IMAGEMAGICK

#include "MagickUtilities.h"
#include "QtUtilities.h"

#include "ImageWriter.h"
#include "Exceptions.h"
#include "Frame.h"
#include "ReaderBase.h"
#include "ZmqLogger.h"

using namespace openshot;

ImageWriter::ImageWriter(std::string path) :
		path(path), cache_size(8), write_video_count(0), image_quality(75), number_of_loops(1),
		combine_frames(true), is_open(false)
{
	info.has_audio = false;
	info.has_video = true;
}

// Set video export options
void ImageWriter::SetVideoOptions(
    std::string format, Fraction fps, int width, int height,
    int quality, int loops, bool combine)
{
    // Set frames per second (if provided)
    info.fps = fps;

    // Set image magic properties
    image_quality = quality;
    number_of_loops = loops;
    combine_frames = combine;
    info.vcodec = format;

    // Set the timebase (inverse of fps)
    info.video_timebase = fps.Reciprocal();

    info.width = std::max(1, width);
    info.height = std::max(1, height);

    info.video_bit_rate = quality;

    // Calculate the DAR (display aspect ratio)
    Fraction size(
        info.width * info.pixel_ratio.num,
        info.height * info.pixel_ratio.den);

    // Reduce size fraction
    size.Reduce();

    // Set the ratio based on the reduced fraction
    info.display_ratio = size;

    ZmqLogger::Instance()->AppendDebugMethod(
        "ImageWriter::SetVideoOptions (" + format + ")",
        "width", width,
        "height", height,
        "size.num", size.num,
        "size.den", size.den,
        "fps.num", fps.num,
        "fps.den", fps.den);
}

// Open the writer
void ImageWriter::Open()
{
	is_open = true;
}

// Add a frame to the queue waiting to be encoded.
void ImageWriter::WriteFrame(std::shared_ptr<Frame> frame)
{
	// Check for open reader (or throw exception)
	if (!is_open) {
		throw WriterClosed(
			"The ImageWriter is closed. "
			"Call Open() before calling this method.", path);
	}

	// Copy and resize image
	auto qimage = frame->GetImage();
	auto frame_image = openshot::QImage2Magick(qimage);
	frame_image->magick( info.vcodec );
	frame_image->backgroundColor(Magick::Color("none"));
	MAGICK_IMAGE_ALPHA(frame_image, true);
	frame_image->quality(image_quality);
	frame_image->animationDelay(info.video_timebase.ToFloat() * 100);
	frame_image->animationIterations(number_of_loops);

	// Calculate correct DAR (display aspect ratio)
	int new_height = info.height * frame->GetPixelRatio().Reciprocal().ToDouble();

	// Resize image
	Magick::Geometry new_size(info.width, new_height);
	new_size.aspect(true);
	frame_image->resize(new_size);


	// Put resized frame in vector (waiting to be written)
	frames.push_back(*frame_image.get());

	// Keep track of the last frame added
	last_frame = frame;
}

// Write a block of frames from a reader
void ImageWriter::WriteFrame(ReaderBase* reader, int64_t start, int64_t length)
{
	ZmqLogger::Instance()->AppendDebugMethod(
		"ImageWriter::WriteFrame (from Reader)",
		"start", start,
		"length", length);

	// Loop through each frame (and encoded it)
	for (int64_t number = start; number <= length; number++)
	{
		// Get the frame
		std::shared_ptr<Frame> f = reader->GetFrame(number);

		// Encode frame
		WriteFrame(f);
	}
}

// Close the writer and encode/output final image to the disk.
void ImageWriter::Close()
{
	// Write frame images to file
	Magick::writeImages(frames.begin(), frames.end(), path, combine_frames);

	// Clear frames vector & counters, close writer
	frames.clear();
	write_video_count = 0;
	is_open = false;

	ZmqLogger::Instance()->AppendDebugMethod("ImageWriter::Close");
}

#endif //USE_IMAGEMAGICK
