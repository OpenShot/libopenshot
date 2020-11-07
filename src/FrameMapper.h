/**
 * @file
 * @brief Header file for the FrameMapper class
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

#ifndef OPENSHOT_FRAMEMAPPER_H
#define OPENSHOT_FRAMEMAPPER_H

#include <assert.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include "CacheMemory.h"
#include "ReaderBase.h"
#include "Frame.h"
#include "Fraction.h"
#include "Exceptions.h"
#include "KeyFrame.h"


// Include FFmpeg headers and macros
#include "FFmpegUtilities.h"
#include "OpenMPUtilities.h"


namespace openshot
{
	/**
	 * @brief This enumeration determines how frame rates are increased or decreased.
	 *
	 * Pull-down techniques are only needed to remove artificial fields added when converting
	 * between 24 fps (film) and television fps (29.97 fps NTSC or 25 fps PAL).
	 */
	enum PulldownType
	{
		PULLDOWN_CLASSIC,	///< Classic 2:3:2:3 pull-down
		PULLDOWN_ADVANCED,	///< Advanced 2:3:3:2 pull-down (minimal dirty frames)
		PULLDOWN_NONE,		///< Do not apply pull-down techniques, just repeat or skip entire frames
	};

	/**
	 * @brief This struct holds a single field (half a frame).
	 *
	 * A frame of video is made up of 2 fields (half a frame).  This struct points to which original
	 * frame, and whether this is the ODD or EVEN lines (i.e. top or bottom).
	 */
	struct Field
	{
		int64_t Frame;
		bool isOdd;

		Field() : Frame(0), isOdd(true) { };

		Field(int64_t frame, bool isodd)
		{
			Frame = frame;
			isOdd = isodd;
		}
	};

	/**
	 * @brief This struct holds a the range of samples needed by this frame
	 *
	 * When frame rate is changed, the audio needs to be redistributed among the remaining
	 * frames.  This struct holds the range samples needed by the this frame.
	 */
	struct SampleRange
	{
		int64_t frame_start;
		int sample_start;

		int64_t frame_end;
		int sample_end;

		int total;
	};

	/**
	 * @brief This struct holds two fields which together make up a complete video frame.
	 *
	 * These fields can point at different original frame numbers, for example the odd lines from
	 * frame 3, and the even lines of frame 4, if required by a pull-down technique.
	 */
	struct MappedFrame
	{
		Field Odd;
		Field Even;
		SampleRange Samples;
	};


	/**
	 * @brief This class creates a mapping between 2 different frame rates, applying a specific pull-down technique.
	 *
	 * This class creates a mapping between 2 different video files, and supports many pull-down techniques,
	 * such as 2:3:2:3 or 2:3:3:2, and also supports inverse telecine. Pull-down techniques are only needed to remove
	 * artificial fields added when converting between 24 fps (film) and television fps (29.97 fps NTSC or 25 fps PAL).
	 *
	 * The <b>following graphic</b> displays a how frame rates are mapped, and how time remapping affects the order
	 * of frames returned from the FrameMapper.
	 * \image html /doc/images/FrameMapper.png
	 *
	 * Please see the following <b>Example Code</b>:
	 * \code
	 * // Create a frame mapper for a reader, and convert the frame rate (from 24 fps to 29.97 fps)
	 * FrameMapper mapping(reader, Fraction(30000, 1001), PULLDOWN_CLASSIC, 44100, 2, LAYOUT_STEREO);
	 * std::shared_ptr<Frame> frame2 = mapping.GetFrame(2);

	 * // If you need to change the mapping...
	 * mapping.ChangeMapping(Fraction(24, 1), PULLDOWN_CLASSIC, 48000, 2, LAYOUT_MONO)
	 * \endcode
	 */
	class FrameMapper : public ReaderBase {
	private:
		bool field_toggle;		// Internal odd / even toggle (used when building the mapping)
		Fraction original;		// The original frame rate
		Fraction target;		// The target frame rate
		PulldownType pulldown;	// The pull-down technique
		ReaderBase *reader;		// The source video reader
		CacheMemory final_cache; 		// Cache of actual Frame objects
		bool is_dirty; 			// When this is true, the next call to GetFrame will re-init the mapping
		SWRCONTEXT *avr;	// Audio resampling context object

		// Internal methods used by init
		void AddField(int64_t frame);
		void AddField(Field field);

		// Get Frame or Generate Blank Frame
		std::shared_ptr<Frame> GetOrCreateFrame(int64_t number);

		/// Adjust frame number for Clip position and start (which can result in a different number)
		int64_t AdjustFrameNumber(int64_t clip_frame_number);

		// Use the original and target frame rates and a pull-down technique to create
		// a mapping between the original fields and frames or a video to a new frame rate.
		// This might repeat or skip fields and frames of the original video, depending on
		// whether the frame rate is increasing or decreasing.
		void Init();

	public:
		// Init some containers
		std::vector<Field> fields;		// List of all fields
		std::vector<MappedFrame> frames;	// List of all frames

		/// Default constructor for openshot::FrameMapper class
		FrameMapper(ReaderBase *reader, Fraction target_fps, PulldownType target_pulldown, int target_sample_rate, int target_channels, ChannelLayout target_channel_layout);

		/// Destructor
		virtual ~FrameMapper();

		/// Change frame rate or audio mapping details
		void ChangeMapping(Fraction target_fps, PulldownType pulldown,  int target_sample_rate, int target_channels, ChannelLayout target_channel_layout);

		/// Close the openshot::FrameMapper and internal reader
		void Close() override;

		/// Get a frame based on the target frame rate and the new frame number of a frame
		MappedFrame GetMappedFrame(int64_t TargetFrameNumber);

		/// Get the cache object used by this reader
		CacheMemory* GetCache() override { return &final_cache; };

		/// @brief This method is required for all derived classes of ReaderBase, and return the
		/// openshot::Frame object, which contains the image and audio information for that
		/// frame of video.
		///
		/// @returns The requested frame of video
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<Frame> GetFrame(int64_t requested_frame) override;

		/// Determine if reader is open or closed
		bool IsOpen() override;

		/// Return the type name of the class
		std::string Name() override { return "FrameMapper"; };

		/// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open the internal reader
		void Open() override;

		/// Print all of the original frames and which new frames they map to
		void PrintMapping();

		/// Get the current reader
		ReaderBase* Reader();

		/// Set the current reader
		void Reader(ReaderBase *new_reader) { reader = new_reader; }

		/// Resample audio and map channels (if needed)
		void ResampleMappedAudio(std::shared_ptr<Frame> frame, int64_t original_frame_number);
	};
}

#endif
