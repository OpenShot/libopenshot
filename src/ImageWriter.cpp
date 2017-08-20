/**
 * @file
 * @brief Source file for ImageWriter class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC, Fabrice Bellard
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
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

#include "../include/ImageWriter.h"

using namespace openshot;

ImageWriter::ImageWriter(string path) throw (InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory) :
		path(path), cache_size(8), is_writing(false), write_video_count(0), image_quality(75), number_of_loops(1),
		combine_frames(true), is_open(false)
{
	// Disable audio & video (so they can be independently enabled)
	info.has_audio = false;
	info.has_video = true;
}

// Set video export options
void ImageWriter::SetVideoOptions(string format, Fraction fps, int width, int height,
		int quality, int loops, bool combine)
{
	// Set frames per second (if provided)
	info.fps.num = fps.num;
	info.fps.den = fps.den;

	// Set image magic properties
	image_quality = quality;
	number_of_loops = loops;
	combine_frames = combine;
	info.vcodec = format;

	// Set the timebase (inverse of fps)
	info.video_timebase.num = info.fps.den;
	info.video_timebase.den = info.fps.num;

	if (width >= 1)
		info.width = width;
	if (height >= 1)
		info.height = height;

	info.video_bit_rate = quality;

	// Calculate the DAR (display aspect ratio)
	Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

	// Reduce size fraction
	size.Reduce();

	// Set the ratio based on the reduced fraction
	info.display_ratio.num = size.num;
	info.display_ratio.den = size.den;

	ZmqLogger::Instance()->AppendDebugMethod("ImageWriter::SetVideoOptions (" + format + ")", "width", width, "height", height, "size.num", size.num, "size.den", size.den, "fps.num", fps.num, "fps.den", fps.den);
}

// Open the writer
void ImageWriter::Open() throw(InvalidFile, InvalidCodec)
{
	is_open = true;
}

// Add a frame to the queue waiting to be encoded.
void ImageWriter::WriteFrame(std::shared_ptr<Frame> frame) throw(WriterClosed)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw WriterClosed("The ImageWriter is closed.  Call Open() before calling this method.", path);


	// Copy and resize image
	std::shared_ptr<Magick::Image> frame_image = frame->GetMagickImage();
	frame_image->magick( info.vcodec );
	frame_image->backgroundColor(Magick::Color("none"));
	frame_image->matte(true);
	frame_image->quality(image_quality);
	frame_image->animationDelay(info.video_timebase.ToFloat() * 100);
	frame_image->animationIterations(number_of_loops);

	// Calculate correct DAR (display aspect ratio)
	int new_width = info.width;
	int new_height = info.height * frame->GetPixelRatio().Reciprocal().ToDouble();

	// Resize image
	Magick::Geometry new_size(new_width, new_height);
	new_size.aspect(true);
	frame_image->resize(new_size);


	// Put resized frame in vector (waiting to be written)
	frames.push_back(*frame_image.get());

	// Keep track of the last frame added
	last_frame = frame;
}

// Write a block of frames from a reader
void ImageWriter::WriteFrame(ReaderBase* reader, long int start, long int length) throw(WriterClosed)
{
	ZmqLogger::Instance()->AppendDebugMethod("ImageWriter::WriteFrame (from Reader)", "start", start, "length", length, "", -1, "", -1, "", -1, "", -1);

	// Loop through each frame (and encoded it)
	for (long int number = start; number <= length; number++)
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
	// Write frame's image to file
	Magick::writeImages(frames.begin(), frames.end(), path, combine_frames);

	// Clear frames vector
	frames.clear();

	// Reset frame counters
	write_video_count = 0;

	// Close writer
	is_open = false;

	ZmqLogger::Instance()->AppendDebugMethod("ImageWriter::Close", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);
}

