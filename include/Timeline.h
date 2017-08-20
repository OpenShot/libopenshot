/**
 * @file
 * @brief Header file for Timeline class
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

#ifndef OPENSHOT_TIMELINE_H
#define OPENSHOT_TIMELINE_H

#include <list>
#include <memory>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include "CacheBase.h"
#include "CacheDisk.h"
#include "CacheMemory.h"
#include "Color.h"
#include "Clip.h"
#include "CrashHandler.h"
#include "Point.h"
#include "EffectBase.h"
#include "Effects.h"
#include "EffectInfo.h"
#include "Fraction.h"
#include "Frame.h"
#include "FrameMapper.h"
#include "KeyFrame.h"
#include "OpenMPUtilities.h"
#include "ReaderBase.h"

using namespace std;
using namespace openshot;

namespace openshot {

	/// Comparison method for sorting clip pointers (by Layer and then Position). Clips are sorted
	/// from lowest layer to top layer (since that is the sequence they need to be combined), and then
	/// by position (left to right).
	struct CompareClips{
		bool operator()( Clip* lhs, Clip* rhs){
			if( lhs->Layer() < rhs->Layer() ) return true;
			if( lhs->Layer() == rhs->Layer() && lhs->Position() <= rhs->Position() ) return true;
			return false;
	}};

	/// Comparison method for sorting effect pointers (by Position, Layer, and Order). Effects are sorted
	/// from lowest layer to top layer (since that is sequence clips are combined), and then by
	/// position, and then by effect order.
	struct CompareEffects{
		bool operator()( EffectBase* lhs, EffectBase* rhs){
			if( lhs->Layer() < rhs->Layer() ) return true;
			if( lhs->Layer() == rhs->Layer() && lhs->Position() < rhs->Position() ) return true;
			if( lhs->Layer() == rhs->Layer() && lhs->Position() == rhs->Position() && lhs->Order() > rhs->Order() ) return true;
			return false;
	}};

	/**
	 * @brief This class represents a timeline
	 *
	 * The timeline is one of the <b>most important</b> features of a video editor, and controls all
	 * aspects of how video, image, and audio clips are combined together, and how the final
	 * video output will be rendered.  It has a collection of layers and clips, that arrange,
	 * sequence, and generate the final video output.
	 *
	 * The <b>following graphic</b> displays a timeline, and how clips can be arranged, scaled, and layered together. It
	 * also demonstrates how the viewport can be scaled smaller than the canvas, which can be used to zoom and pan around the
	 * canvas (i.e. pan & scan).
	 * \image html /doc/images/Timeline_Layers.png
	 *
	 * The <b>following graphic</b> displays how the playhead determines which frames to combine and layer.
	 * \image html /doc/images/Playhead.png
	 *
	 * Lets take a look at what the code looks like:
	 * @code
	 * // Create a Timeline
	 * Timeline t(1280, // width
	 *            720, // height
	 *            Fraction(25,1), // framerate
	 *            44100, // sample rate
	 *            2 // channels
	 *            );
	 *
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
	 *
	 * // Add clips to timeline
	 * t.AddClip(&c1);
	 * t.AddClip(&c2);
	 *
	 * // Open the timeline reader
	 * t.Open();
	 *
	 * // Get frame number 1 from the timeline (This will generate a new frame, made up from the previous clips and settings)
	 * std::shared_ptr<Frame> f = t.GetFrame(1);
	 *
	 * // Now that we have an openshot::Frame object, lets have some fun!
	 * f->Display(); // Display the frame on the screen
	 *
	 * // Close the timeline reader
	 * t.Close();
	 * @endcode
	 */
	class Timeline : public ReaderBase {
	private:
		bool is_open; ///<Is Timeline Open?
		bool auto_map_clips; ///< Auto map framerates and sample rates to all clips
		list<Clip*> clips; ///<List of clips on this timeline
		list<Clip*> closing_clips; ///<List of clips that need to be closed
		map<Clip*, Clip*> open_clips; ///<List of 'opened' clips on this timeline
		list<EffectBase*> effects; ///<List of clips on this timeline
		CacheBase *final_cache; ///<Final cache of timeline frames

		/// Process a new layer of video or audio
		void add_layer(std::shared_ptr<Frame> new_frame, Clip* source_clip, long int clip_frame_number, long int timeline_frame_number, bool is_top_clip);

		/// Apply a FrameMapper to a clip which matches the settings of this timeline
		void apply_mapper_to_clip(Clip* clip);

		/// Apply JSON Diffs to various objects contained in this timeline
		void apply_json_to_clips(Json::Value change) throw(InvalidJSONKey); ///<Apply JSON diff to clips
		void apply_json_to_effects(Json::Value change) throw(InvalidJSONKey); ///< Apply JSON diff to effects
		void apply_json_to_effects(Json::Value change, EffectBase* existing_effect) throw(InvalidJSONKey); ///<Apply JSON diff to a specific effect
		void apply_json_to_timeline(Json::Value change) throw(InvalidJSONKey); ///<Apply JSON diff to timeline properties

		/// Calculate time of a frame number, based on a framerate
		double calculate_time(long int number, Fraction rate);

		/// Find intersecting (or non-intersecting) openshot::Clip objects
		///
		/// @returns A list of openshot::Clip objects
		/// @param requested_frame The frame number that is requested.
		/// @param number_of_frames The number of frames to check
		/// @param include Include or Exclude intersecting clips
		vector<Clip*> find_intersecting_clips(long int requested_frame, int number_of_frames, bool include);

		/// Get or generate a blank frame
		std::shared_ptr<Frame> GetOrCreateFrame(Clip* clip, long int number);

		/// Apply effects to the source frame (if any)
		std::shared_ptr<Frame> apply_effects(std::shared_ptr<Frame> frame, long int timeline_frame_number, int layer);

		/// Compare 2 floating point numbers for equality
		bool isEqual(double a, double b);

		/// Sort clips by position on the timeline
		void sort_clips();

		/// Sort effects by position on the timeline
		void sort_effects();

		/// Update the list of 'opened' clips
		void update_open_clips(Clip *clip, bool does_clip_intersect);

	public:

		/// @brief Default Constructor for the timeline (which sets the canvas width and height and FPS)
		/// @param width The width of the timeline (and thus, the generated openshot::Frame objects)
		/// @param height The height of the timeline (and thus, the generated openshot::Frame objects)
		/// @param fps The frames rate of the timeline
		/// @param sample_rate The sample rate of the timeline's audio
		/// @param channels The number of audio channels of the timeline
		/// @param channel_layout The channel layout (i.e. mono, stereo, 3 point surround, etc...)
		Timeline(int width, int height, Fraction fps, int sample_rate, int channels, ChannelLayout channel_layout);

		/// @brief Add an openshot::Clip to the timeline
		/// @param clip Add an openshot::Clip to the timeline. A clip can contain any type of Reader.
		void AddClip(Clip* clip) throw(ReaderClosed);

		/// @brief Add an effect to the timeline
		/// @param effect Add an effect to the timeline. An effect can modify the audio or video of an openshot::Frame.
		void AddEffect(EffectBase* effect);

		/// Apply the timeline's framerate and samplerate to all clips
		void ApplyMapperToClips();

		/// Determine if clips are automatically mapped to the timeline's framerate and samplerate
		bool AutoMapClips() { return auto_map_clips; };

		/// @brief Automatically map all clips to the timeline's framerate and samplerate
		void AutoMapClips(bool auto_map) { auto_map_clips = auto_map; };

        /// Clear all cache for this timeline instance, and all clips, mappers, and readers under it
        void ClearAllCache();

		/// Return a list of clips on the timeline
		list<Clip*> Clips() { return clips; };

		/// Close the timeline reader (and any resources it was consuming)
		void Close();

		/// Return the list of effects on the timeline
		list<EffectBase*> Effects() { return effects; };

		/// Get the cache object used by this reader
		CacheBase* GetCache() { return final_cache; };

		/// Get the cache object used by this reader
		void SetCache(CacheBase* new_cache);

		/// Get an openshot::Frame object for a specific frame number of this timeline.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<Frame> GetFrame(long int requested_frame) throw(ReaderClosed, OutOfBoundsFrame);

		// Curves for the viewport
		Keyframe viewport_scale; ///<Curve representing the scale of the viewport (0 to 100)
		Keyframe viewport_x; ///<Curve representing the x coordinate for the viewport
		Keyframe viewport_y; ///<Curve representing the y coordinate for the viewport

		// Background color
		Color color; ///<Background color of timeline canvas

		/// Determine if reader is open or closed
		bool IsOpen() { return is_open; };

		/// Return the type name of the class
		string Name() { return "Timeline"; };

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root) throw(InvalidFile, ReaderClosed); ///< Load Json::JsonValue into this object

		/// @brief Apply a special formatted JSON object, which represents a change to the timeline (add, update, delete)
		/// This is primarily designed to keep the timeline (and its child objects... such as clips and effects) in sync
		/// with another application... such as OpenShot Video Editor (http://www.openshot.org).
		/// @param value A JSON string containing a key, value, and type of change.
		void ApplyJsonDiff(string value) throw(InvalidJSON, InvalidJSONKey);

		/// Open the reader (and start consuming resources)
		void Open();

		/// @brief Remove an openshot::Clip from the timeline
		/// @param clip Remove an openshot::Clip from the timeline.
		void RemoveClip(Clip* clip);

		/// @brief Remove an effect from the timeline
		/// @param effect Remove an effect from the timeline.
		void RemoveEffect(EffectBase* effect);
	};


}

#endif
