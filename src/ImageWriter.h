/**
 * @file
 * @brief Header file for ImageWriter class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC, Fabrice Bellard
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_IMAGE_WRITER_H
#define OPENSHOT_IMAGE_WRITER_H

#ifdef USE_IMAGEMAGICK

#include <string>
#include <vector>

#include "WriterBase.h"
#include "MagickUtilities.h"

#include "Fraction.h"

namespace openshot
{
	// Forward decls
	class Frame;
	class ReaderBase;

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
		std::string path;
		int cache_size;
		bool is_open;
		int64_t write_video_count;
		std::vector<Magick::Image> frames;
		int image_quality;
		int number_of_loops;
		bool combine_frames;

		std::shared_ptr<Frame> last_frame;

	public:

		/// @brief Constructor for ImageWriter. Throws one of the following exceptions.
		/// @param path The path of the file you want to create
		ImageWriter(std::string path);

		/// @brief Close the writer and encode/output final image to the disk. This is a requirement of ImageMagick,
		/// which writes all frames of a multi-frame image at one time.
		void Close();

		/// @brief Get the cache size
		int GetCacheSize() { return cache_size; };

		/// Determine if writer is open or closed
		bool IsOpen() { return is_open; };

		/// Open writer
		void Open();

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
		/// @param combine Combine frames into a single image (if possible), or save each frame as its own image
		void SetVideoOptions(
			std::string format, Fraction fps, int width, int height,
			int quality, int loops, bool combine);

		/// @brief Add a frame to the stack waiting to be encoded.
		/// @param frame The openshot::Frame object to write to this image
		void WriteFrame(std::shared_ptr<Frame> frame);

		/// @brief Write a block of frames from a reader
		/// @param reader A openshot::ReaderBase object which will provide frames to be written
		/// @param start The starting frame number of the reader
		/// @param length The number of frames to write
		void WriteFrame(ReaderBase* reader, int64_t start, int64_t length);

	};

}  // namespace

#endif  //USE_IMAGEMAGICK
#endif  //OPENSHOT_IMAGE_WRITER_H
