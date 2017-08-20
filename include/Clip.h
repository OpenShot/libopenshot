/**
 * @file
 * @brief Header file for Clip class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

#ifndef OPENSHOT_CLIP_H
#define OPENSHOT_CLIP_H

/// Do not include the juce unittest headers, because it collides with unittest++
#ifndef __JUCE_UNITTEST_JUCEHEADER__
	#define __JUCE_UNITTEST_JUCEHEADER__
#endif

#include <memory>
#include <string>
#include <QtGui/QImage>
#include "JuceLibraryCode/JuceHeader.h"
#include "AudioResampler.h"
#include "ClipBase.h"
#include "Color.h"
#include "Enums.h"
#include "EffectBase.h"
#include "Effects.h"
#include "EffectInfo.h"
#include "FFmpegReader.h"
#include "Fraction.h"
#include "FrameMapper.h"
#ifdef USE_IMAGEMAGICK
	#include "ImageReader.h"
	#include "TextReader.h"
#endif
#include "QtImageReader.h"
#include "ChunkReader.h"
#include "KeyFrame.h"
#include "ReaderBase.h"
#include "DummyReader.h"

using namespace std;
using namespace openshot;

namespace openshot {

	/// Comparison method for sorting effect pointers (by Position, Layer, and Order). Effects are sorted
	/// from lowest layer to top layer (since that is sequence clips are combined), and then by
	/// position, and then by effect order.
	struct CompareClipEffects{
		bool operator()( EffectBase* lhs, EffectBase* rhs){
			if( lhs->Layer() < rhs->Layer() ) return true;
			if( lhs->Layer() == rhs->Layer() && lhs->Position() < rhs->Position() ) return true;
			if( lhs->Layer() == rhs->Layer() && lhs->Position() == rhs->Position() && lhs->Order() > rhs->Order() ) return true;
			return false;
	}};

	/**
	 * @brief This class represents a clip (used to arrange readers on the timeline)
	 *
	 * Each image, video, or audio file is represented on a layer as a clip.  A clip has many
	 * properties that affect how it behaves on the timeline, such as its size, position,
	 * transparency, rotation, speed, volume, etc...
	 *
	 * @code
	 * // Create some clips
	 * Clip c1(new ImageReader("MyAwesomeLogo.jpeg"));
	 * Clip c2(new FFmpegReader("BackgroundVideo.webm"));
	 *
	 * // CLIP 1 (logo) - Set some clip properties (with Keyframes)
	 * c1.Position(0.0); // Set the position or location (in seconds) on the timeline
	 * c1.gravity = GRAVITY_LEFT; // Set the alignment / gravity of the clip (position on the screen)
	 * c1.scale = SCALE_CROP; // Set the scale mode (how the image is resized to fill the screen)
	 * c1.Layer(1); // Set the layer of the timeline (higher layers cover up images of lower layers)
	 * c1.Start(0.0); // Set the starting position of the video (trim the left side of the video)
	 * c1.End(16.0); // Set the ending position of the video (trim the right side of the video)
	 * c1.alpha.AddPoint(1, 0.0); // Set the alpha to transparent on frame #1
	 * c1.alpha.AddPoint(500, 0.0); // Keep the alpha transparent until frame #500
	 * c1.alpha.AddPoint(565, 1.0); // Animate the alpha from transparent to visible (between frame #501 and #565)
	 *
	 * // CLIP 2 (background video) - Set some clip properties (with Keyframes)
	 * c2.Position(0.0); // Set the position or location (in seconds) on the timeline
	 * c2.Start(10.0); // Set the starting position of the video (trim the left side of the video)
	 * c2.Layer(0); // Set the layer of the timeline (higher layers cover up images of lower layers)
	 * c2.alpha.AddPoint(1, 1.0); // Set the alpha to visible on frame #1
	 * c2.alpha.AddPoint(150, 0.0); // Animate the alpha to transparent (between frame 2 and frame #150)
	 * c2.alpha.AddPoint(360, 0.0, LINEAR); // Keep the alpha transparent until frame #360
	 * c2.alpha.AddPoint(384, 1.0); // Animate the alpha to visible (between frame #360 and frame #384)
	 * @endcode
	 */
	class Clip : public ClipBase {
	protected:
		/// Section lock for multiple threads
	    CriticalSection getFrameCriticalSection;

	private:
		bool waveform; ///< Should a waveform be used instead of the clip's image
		list<EffectBase*> effects; ///<List of clips on this timeline

		// Audio resampler (if time mapping)
		AudioResampler *resampler;
		AudioSampleBuffer *audio_cache;

		// File Reader object
		ReaderBase* reader;
		bool manage_reader;

		/// Adjust frame number minimum value
		long int adjust_frame_number_minimum(long int frame_number);

		/// Apply effects to the source frame (if any)
		std::shared_ptr<Frame> apply_effects(std::shared_ptr<Frame> frame);

		/// Get file extension
		string get_file_extension(string path);

		/// Get a frame object or create a blank one
		std::shared_ptr<Frame> GetOrCreateFrame(long int number);

		/// Adjust the audio and image of a time mapped frame
		std::shared_ptr<Frame> get_time_mapped_frame(std::shared_ptr<Frame> frame, long int frame_number) throw(ReaderClosed);

		/// Init default settings for a clip
		void init_settings();

		/// Sort effects by order
		void sort_effects();

		/// Reverse an audio buffer
		void reverse_buffer(juce::AudioSampleBuffer* buffer);

	public:
		GravityType gravity; ///< The gravity of a clip determines where it snaps to it's parent
		ScaleType scale; ///< The scale determines how a clip should be resized to fit it's parent
		AnchorType anchor; ///< The anchor determines what parent a clip should snap to
        FrameDisplayType display; ///< The format to display the frame number (if any)

		/// Default Constructor
		Clip();

		/// @brief Constructor with filepath (reader is automatically created... by guessing file extensions)
		/// @param path The path of a reader (video file, image file, etc...). The correct reader will be used automatically.
		Clip(string path);

		/// @brief Constructor with reader
		/// @param new_reader The reader to be used by this clip
		Clip(ReaderBase* new_reader);

		/// Destructor
		~Clip();

		/// @brief Add an effect to the clip
		/// @param effect Add an effect to the clip. An effect can modify the audio or video of an openshot::Frame.
		void AddEffect(EffectBase* effect);

		/// Close the internal reader
		void Close() throw(ReaderClosed);

		/// Return the list of effects on the timeline
		list<EffectBase*> Effects() { return effects; };

		/// @brief Get an openshot::Frame object for a specific frame number of this timeline.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested
		std::shared_ptr<Frame> GetFrame(long int requested_frame) throw(ReaderClosed);

		/// Open the internal reader
		void Open() throw(InvalidFile, ReaderClosed);

		/// @brief Set the current reader
		/// @param new_reader The reader to be used by this clip
		void Reader(ReaderBase* new_reader);

		/// Get the current reader
		ReaderBase* Reader() throw(ReaderClosed);

		/// Override End() method
		float End() throw(ReaderClosed); ///< Get end position (in seconds) of clip (trim end of video), which can be affected by the time curve.
		void End(float value) { end = value; } ///< Set end position (in seconds) of clip (trim end of video)

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		string PropertiesJSON(long int requested_frame);

		/// @brief Remove an effect from the clip
		/// @param effect Remove an effect from the clip.
		void RemoveEffect(EffectBase* effect);

		/// Waveform property
		bool Waveform() { return waveform; } ///< Get the waveform property of this clip
		void Waveform(bool value) { waveform = value; } ///< Set the waveform property of this clip

		// Scale and Location curves
		Keyframe scale_x; ///< Curve representing the horizontal scaling in percent (0 to 1)
		Keyframe scale_y; ///< Curve representing the vertical scaling in percent (0 to 1)
		Keyframe location_x; ///< Curve representing the relative X position in percent based on the gravity (-1 to 1)
		Keyframe location_y; ///< Curve representing the relative Y position in percent based on the gravity (-1 to 1)

		// Alpha and Rotation curves
		Keyframe alpha; ///< Curve representing the alpha (1 to 0)
		Keyframe rotation; ///< Curve representing the rotation (0 to 360)

		// Time and Volume curves
		Keyframe time; ///< Curve representing the frames over time to play (used for speed and direction of video)
		Keyframe volume; ///< Curve representing the volume (0 to 1)

		/// Curve representing the color of the audio wave form
		Color wave_color;

		// Crop settings and curves
		GravityType crop_gravity; ///< Cropping needs to have a gravity to determine what side we are cropping
		Keyframe crop_width; ///< Curve representing width in percent (0.0=0%, 1.0=100%)
		Keyframe crop_height; ///< Curve representing height in percent (0.0=0%, 1.0=100%)
		Keyframe crop_x; ///< Curve representing X offset in percent (-1.0=-100%, 0.0=0%, 1.0=100%)
		Keyframe crop_y; ///< Curve representing Y offset in percent (-1.0=-100%, 0.0=0%, 1.0=100%)

		// Shear and perspective curves
		Keyframe shear_x; ///< Curve representing X shear angle in degrees (-45.0=left, 45.0=right)
		Keyframe shear_y; ///< Curve representing Y shear angle in degrees (-45.0=down, 45.0=up)
		Keyframe perspective_c1_x; ///< Curves representing X for coordinate 1
		Keyframe perspective_c1_y; ///< Curves representing Y for coordinate 1
		Keyframe perspective_c2_x; ///< Curves representing X for coordinate 2
		Keyframe perspective_c2_y; ///< Curves representing Y for coordinate 2
		Keyframe perspective_c3_x; ///< Curves representing X for coordinate 3
		Keyframe perspective_c3_y; ///< Curves representing Y for coordinate 3
		Keyframe perspective_c4_x; ///< Curves representing X for coordinate 4
		Keyframe perspective_c4_y; ///< Curves representing Y for coordinate 4

		/// Audio channel filter and mappings
		Keyframe channel_filter; ///< A number representing an audio channel to filter (clears all other channels)
		Keyframe channel_mapping; ///< A number representing an audio channel to output (only works when filtering a channel)

		/// Override has_video and has_audio properties of clip (and their readers)
		Keyframe has_audio; ///< An optional override to determine if this clip has audio (-1=undefined, 0=no, 1=yes)
		Keyframe has_video; ///< An optional override to determine if this clip has video (-1=undefined, 0=no, 1=yes)
	};


}

#endif
