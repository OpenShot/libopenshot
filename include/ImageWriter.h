/**
 * @file
 * @brief Header file for ImageWriter class
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
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * * OpenShot Library (libopenshot) is free software: you can redistribute it
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


#ifndef OPENSHOT_IMAGE_WRITER_H
#define OPENSHOT_IMAGE_WRITER_H

#include "ReaderBase.h"
#include "WriterBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "Magick++.h"
#include "CacheMemory.h"
#include "Exceptions.h"
#include "OpenMPUtilities.h"


using namespace std;

namespace openshot
{

	/**
	 * @brief This class uses the ImageMagick library to write image files (including animated GIFs)
	 *
	 * All image formats supported by ImageMagick are supported by this class.
	 *
	 * @code
	 * // Create a reader for a video
	 * FFmpegReader r("MyAwesomeVideo.webm");
	 * r.Open(); // Open the reader
	 *
	 * // Create a writer (which will create an animated GIF file)
	 * ImageWriter w("/home/jonathan/NewAnimation.gif");
	 *
	 * // Set the image output settings (format, fps, width, height, quality, loops, combine)
	 * w.SetVideoOptions("GIF", r.info.fps, r.info.width, r.info.height, 70, 1, true);
	 *
	 * // Open the writer
	 * w.Open();
	 *
	 * // Write the 1st 30 frames from the reader
	 * w.WriteFrame(&r, 1, 30);
	 *
	 * // Close the reader & writer
	 * w.Close();
	 * r.Close();
	 * @endcode
	 */
	class ImageWriter : public WriterBase
	{
	private:
		string path;
		int cache_size;
		bool is_writing;
		bool is_open;
		int64 write_video_count;
		vector<Magick::Image> frames;
		int image_quality;
		int number_of_loops;
		bool combine_frames;

	    std::shared_ptr<Frame> last_frame;

	public:

		/// @brief Constructor for ImageWriter. Throws one of the following exceptions.
		/// @param path The path of the file you want to create
		ImageWriter(string path) throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory);

		/// @brief Close the writer and encode/output final image to the disk. This is a requirement of ImageMagick,
		/// which writes all frames of a multi-frame image at one time.
		void Close();

		/// @brief Get the cache size
		int GetCacheSize() { return cache_size; };

		/// Determine if writer is open or closed
		bool IsOpen() { return is_open; };

		/// Open writer
		void Open() throw(InvalidFile, InvalidCodec);

		/// @brief Set the cache size (number of frames to queue before writing)
		/// @param new_size Number of frames to queue before writing
		void SetCacheSize(int new_size) { cache_size = new_size; };

		/// @brief Set the video export options
		/// @param format The image format (such as GIF)
		/// @param fps Frames per second of the image (used on certain multi-frame image formats, such as GIF)
		/// @param width Width in pixels of image
		/// @param height Height in pixels of image
		/// @param quality Quality of image (0 to 100, 70 is default)
		/// @param loops Number of times to repeat the image (used on certain multi-frame image formats, such as GIF)
		/// @param combine Combine frames into a single image (if possible), or save each frame as it's own image
		void SetVideoOptions(string format, Fraction fps, int width, int height,
				int quality, int loops, bool combine);

		/// @brief Add a frame to the stack waiting to be encoded.
		/// @param frame The openshot::Frame object to write to this image
		void WriteFrame(std::shared_ptr<Frame> frame) throw(WriterClosed);

		/// @brief Write a block of frames from a reader
		/// @param reader A openshot::ReaderBase object which will provide frames to be written
		/// @param start The starting frame number of the reader
		/// @param length The number of frames to write
		void WriteFrame(ReaderBase* reader, long int start, long int length) throw(WriterClosed);

	};

}

#endif
