/**
 * @file
 * @brief Header file for DummyReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
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

#ifndef OPENSHOT_DUMMY_READER_H
#define OPENSHOT_DUMMY_READER_H

#include "ReaderBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <memory>
#include "CacheMemory.h"
#include "Exceptions.h"
#include "Fraction.h"

namespace openshot
{
	/**
	 * @brief This class is used as a simple, dummy reader, which can be very useful when writing
	 * unit tests. It can return a single blank frame or it can return custom frame objects
	 * which were passed into the constructor with a Cache object.
	 *
	 * A dummy reader can be created with any framerate or samplerate. This is useful in unit
	 * tests that need to test different framerates or samplerates.
	 *
	 * @note Timeline does buffering by requesting more frames than it
	 * strictly needs. Thus if you use this DummyReader with a custom
	 * cache in a Timeline, make sure it has enough
	 * frames. Specifically you need some frames after the last frame
	 * you plan to access through the Timeline.
	 *
	 * @code
	 * 	// Create cache object to store fake Frame objects
	 * 	CacheMemory cache;
	 *
	 * // Now let's create some test frames
	 * for (int64_t frame_number = 1; frame_number <= 30; frame_number++)
	 * {
	 *   // Create blank frame (with specific frame #, samples, and channels)
	 *   // Sample count should be 44100 / 30 fps = 1470 samples per frame
	 *   int sample_count = 1470;
	 *   auto f = std::make_shared<openshot::Frame>(frame_number, sample_count, 2);
	 *
	 *   // Create test samples with incrementing value
	 *   float *audio_buffer = new float[sample_count];
	 *   for (int64_t sample_number = 0; sample_number < sample_count; sample_number++)
	 *   {
	 *     // Generate an incrementing audio sample value (just as an example)
	 *     audio_buffer[sample_number] = float(frame_number) + (float(sample_number) / float(sample_count));
	 *   }
	 *
	 *   // Add custom audio samples to Frame (bool replaceSamples, int destChannel, int destStartSample, const float* source,
	 *   //                                    int numSamples, float gainToApplyToSource = 1.0f)
	 *   f->AddAudio(true, 0, 0, audio_buffer, sample_count, 1.0); // add channel 1
	 *   f->AddAudio(true, 1, 0, audio_buffer, sample_count, 1.0); // add channel 2
	 *
	 *   // Add test frame to cache
	 *   cache.Add(f);
	 * }
	 *
	 * // Create a reader (Fraction fps, int width, int height, int sample_rate, int channels, float duration, CacheBase* cache)
	 * openshot::DummyReader r(openshot::Fraction(30, 1), 1920, 1080, 44100, 2, 30.0, &cache);
	 * r.Open(); // Open the reader
	 *
	 * // Now let's verify our DummyReader works
	 * std::shared_ptr<openshot::Frame> f = r.GetFrame(1);
	 * // r.GetFrame(1)->GetAudioSamples(0)[1] should equal 1.00068033 based on our above calculations
	 *
	 * // Clean up
	 * r.Close();
	 * cache.Clear()
	 * @endcode
	 */
	class DummyReader : public ReaderBase
	{
	private:
		CacheBase* dummy_cache;
		std::shared_ptr<openshot::Frame> image_frame;
		bool is_open;

		/// Initialize variables used by constructor
		void init(Fraction fps, int width, int height, int sample_rate, int channels, float duration);

	public:

		/// Blank constructor for DummyReader, with default settings.
		DummyReader();

		/// Constructor for DummyReader.
		DummyReader(openshot::Fraction fps, int width, int height, int sample_rate, int channels, float duration);

		/// Constructor for DummyReader which takes a frame cache object.
		DummyReader(openshot::Fraction fps, int width, int height, int sample_rate, int channels, float duration, CacheBase* cache);

		virtual ~DummyReader();

		/// Close File
		void Close() override;

		/// Get the cache object used by this reader (always returns NULL for this reader)
		CacheMemory* GetCache() override { return NULL; };

		/// Get an openshot::Frame object for a specific frame number of this reader.  All numbers
		/// return the same Frame, since they all share the same image data.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t requested_frame) override;

		/// Determine if reader is open or closed
		bool IsOpen() override { return is_open; };

		/// Return the type name of the class
		std::string Name() override { return "DummyReader"; };

		/// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open File - which is called by the constructor automatically
		void Open() override;
	};

}

#endif
