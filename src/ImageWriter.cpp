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

	#pragma omp critical (debug_output)
	AppendDebugMethod("ImageWriter::SetVideoOptions (" + format + ")", "width", width, "height", height, "size.num", size.num, "size.den", size.den, "fps.num", fps.num, "fps.den", fps.den);
}

// Open the writer
void ImageWriter::Open() throw(InvalidFile, InvalidCodec)
{
	is_open = true;
}

// Add a frame to the queue waiting to be encoded.
void ImageWriter::WriteFrame(tr1::shared_ptr<Frame> frame) throw(WriterClosed)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw WriterClosed("The FFmpegWriter is closed.  Call Open() before calling this method.", path);

	// Add frame pointer to "queue", waiting to be processed the next
	// time the WriteFrames() method is called.
	spooled_video_frames.push_back(frame);

	#pragma omp critical (debug_output)
	AppendDebugMethod("ImageWriter::WriteFrame", "frame->number", frame->number, "spooled_video_frames.size()", spooled_video_frames.size(), "cache_size", cache_size, "is_writing", is_writing, "", -1, "", -1);

	// Write the frames once it reaches the correct cache size
	if (spooled_video_frames.size() == cache_size)
	{
		// Is writer currently writing?
		if (!is_writing)
			// Write frames to video file
			write_queued_frames();

		else
		{
			// YES, WRITING... so wait until it finishes, before writing again
			while (is_writing)
				Sleep(1); // sleep for 250 milliseconds

			// Write frames to video file
			write_queued_frames();
		}
	}

	// Keep track of the last frame added
	last_frame = frame;
}

// Write all frames in the queue to the video file.
void ImageWriter::write_queued_frames()
{
	#pragma omp critical (debug_output)
	AppendDebugMethod("ImageWriter::write_queued_frames", "spooled_video_frames.size()", spooled_video_frames.size(), "", -1, "", -1, "", -1, "", -1, "", -1);

	// Flip writing flag
	is_writing = true;

	// Transfer spool to queue
	queued_video_frames = spooled_video_frames;

	// Empty spool
	spooled_video_frames.clear();

	// Set the number of threads in OpenMP
	omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
	// Allow nested OpenMP sections
	omp_set_nested(true);

	#pragma omp parallel
	{
		#pragma omp single
		{
			// Loop through each queued image frame
			while (!queued_video_frames.empty())
			{
				// Get front frame (from the queue)
				tr1::shared_ptr<Frame> frame = queued_video_frames.front();

				// Add to processed queue
				processed_frames.push_back(frame);

				// Copy and resize image
				tr1::shared_ptr<Magick::Image> frame_image = frame->GetImage();
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

				// Remove front item
				queued_video_frames.pop_front();

			} // end while
		} // end omp single

		#pragma omp single
		{
			// Loop back through the frames (in order), and write them to the video file
			while (!processed_frames.empty())
			{
				// Get front frame (from the queue)
				tr1::shared_ptr<Frame> frame = processed_frames.front();

				// Add to deallocate queue (so we can remove the AVFrames when we are done)
				deallocate_frames.push_back(frame);

				// Write frame to video file
				// write_video_packet(frame, frame_final);

				// Remove front item
				processed_frames.pop_front();
			}

			// Loop through, and deallocate AVFrames
			while (!deallocate_frames.empty())
			{
				// Get front frame (from the queue)
				tr1::shared_ptr<Frame> frame = deallocate_frames.front();

				// Remove front item
				deallocate_frames.pop_front();
			}

			// Done writing
			is_writing = false;

		} // end omp single
	} // end omp parallel

}

// Write a block of frames from a reader
void ImageWriter::WriteFrame(ReaderBase* reader, int start, int length) throw(WriterClosed)
{
	#pragma omp critical (debug_output)
	AppendDebugMethod("ImageWriter::WriteFrame (from Reader)", "start", start, "length", length, "", -1, "", -1, "", -1, "", -1);

	// Loop through each frame (and encoded it)
	for (int number = start; number <= length; number++)
	{
		// Get the frame
		tr1::shared_ptr<Frame> f = reader->GetFrame(number);

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

	#pragma omp critical (debug_output)
	AppendDebugMethod("ImageWriter::Close", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);
}

