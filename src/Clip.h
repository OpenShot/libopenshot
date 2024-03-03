/**
 * @file
 * @brief Header file for Clip class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CLIP_H
#define OPENSHOT_CLIP_H

#ifdef USE_OPENCV
	#define int64 opencv_broken_int
	#define uint64 opencv_broken_uint
	#include <opencv2/opencv.hpp>
	#include <opencv2/core.hpp>
	#undef uint64
	#undef int64

#endif

#include <memory>
#include <string>

#include "AudioLocation.h"
#include "ClipBase.h"
#include "ReaderBase.h"

#include "Color.h"
#include "Enums.h"
#include "EffectBase.h"
#include "EffectInfo.h"
#include "KeyFrame.h"
#include "TrackedObjectBase.h"

namespace openshot {
	class AudioResampler;
	class EffectInfo;
	class Frame;

	/// Comparison method for sorting effect pointers (by Position, Layer, and Order). Effects are sorted
	/// from lowest layer to top layer (since that is sequence clips are combined), and then by
	/// position, and then by effect order.
	struct CompareClipEffects{
		bool operator()( openshot::EffectBase* lhs, openshot::EffectBase* rhs){
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
	 * // CLIP 1 (logo) - Set some clip properties (with openshot::Keyframes)
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
	 * // CLIP 2 (background video) - Set some clip properties (with openshot::Keyframes)
	 * c2.Position(0.0); // Set the position or location (in seconds) on the timeline
	 * c2.Start(10.0); // Set the starting position of the video (trim the left side of the video)
	 * c2.Layer(0); // Set the layer of the timeline (higher layers cover up images of lower layers)
	 * c2.alpha.AddPoint(1, 1.0); // Set the alpha to visible on frame #1
	 * c2.alpha.AddPoint(150, 0.0); // Animate the alpha to transparent (between frame 2 and frame #150)
	 * c2.alpha.AddPoint(360, 0.0, LINEAR); // Keep the alpha transparent until frame #360
	 * c2.alpha.AddPoint(384, 1.0); // Animate the alpha to visible (between frame #360 and frame #384)
	 * @endcode
	 */
	class Clip : public openshot::ClipBase, public openshot::ReaderBase {
	protected:
		/// Mutex for multiple threads
		std::recursive_mutex getFrameMutex;

		/// Previous time-mapped audio location
		AudioLocation previous_location;

		/// Init default settings for a clip
		void init_settings();

		/// Init reader info details
		void init_reader_settings();

		/// Update default rotation from reader
		void init_reader_rotation();

	private:
		bool waveform; ///< Should a waveform be used instead of the clip's image
		std::list<openshot::EffectBase*> effects; ///< List of clips on this timeline
		bool is_open;	///< Is Reader opened
		std::string parentObjectId; ///< Id of the bounding box that this clip is attached to
		std::shared_ptr<openshot::TrackedObjectBase> parentTrackedObject; ///< Tracked object this clip is attached to
		openshot::Clip* parentClipObject; ///< Clip object this clip is attached to

		/// Final cache object used to hold final frames
		CacheMemory final_cache;
		
		// Audio resampler (if time mapping)
		openshot::AudioResampler *resampler;

		// File Reader object
		openshot::ReaderBase* reader;

		/// If we allocated a reader, we store it here to free it later
		/// (reader member variable itself may have been replaced)
		openshot::ReaderBase* allocated_reader;

		/// Adjust frame number minimum value
		int64_t adjust_frame_number_minimum(int64_t frame_number);

		/// Apply background image to the current clip image (i.e. flatten this image onto previous layer)
		void apply_background(std::shared_ptr<openshot::Frame> frame, std::shared_ptr<openshot::Frame> background_frame);

		/// Apply effects to the source frame (if any)
		void apply_effects(std::shared_ptr<openshot::Frame> frame, int64_t timeline_frame_number, TimelineInfoStruct* options, bool before_keyframes);

		/// Apply keyframes to an openshot::Frame and use an existing background frame (if any)
		void apply_keyframes(std::shared_ptr<Frame> frame, QSize timeline_size);

		/// Apply waveform image to an openshot::Frame and use an existing background frame (if any)
		void apply_waveform(std::shared_ptr<Frame> frame, QSize timeline_size);

		/// Adjust frame number for Clip position and start (which can result in a different number)
		int64_t adjust_timeline_framenumber(int64_t clip_frame_number);
		
		/// Get QTransform from keyframes
		QTransform get_transform(std::shared_ptr<Frame> frame, int width, int height);

		/// Get file extension
		std::string get_file_extension(std::string path);

		/// Get a frame object or create a blank one
		std::shared_ptr<openshot::Frame> GetOrCreateFrame(int64_t number, bool enable_time=true);

		/// Adjust the audio and image of a time mapped frame
		void apply_timemapping(std::shared_ptr<openshot::Frame> frame);

		/// Compare 2 floating point numbers and return true if they are extremely close
		bool isNear(double a, double b);

		/// Sort effects by order
		void sort_effects();

		/// Scale a source size to a target size (given a specific scale-type)
		QSize scale_size(QSize source_size, ScaleType source_scale, int target_width, int target_height);

	public:
		openshot::GravityType gravity;   ///< The gravity of a clip determines where it snaps to its parent
		openshot::ScaleType scale;		 ///< The scale determines how a clip should be resized to fit its parent
		openshot::AnchorType anchor;	 ///< The anchor determines what parent a clip should snap to
		openshot::FrameDisplayType display; ///< The format to display the frame number (if any)
		openshot::VolumeMixType mixing;  ///< What strategy should be followed when mixing audio with other clips

		#ifdef USE_OPENCV
			bool COMPILED_WITH_CV = true;
		#else
			bool COMPILED_WITH_CV = false;
		#endif

		/// Default Constructor
		Clip();

		/// @brief Constructor with filepath (reader is automatically created... by guessing file extensions)
		/// @param path The path of a reader (video file, image file, etc...). The correct reader will be used automatically.
		Clip(std::string path);

		/// @brief Constructor with reader
		/// @param new_reader The reader to be used by this clip
		Clip(openshot::ReaderBase* new_reader);

		/// Destructor
		virtual ~Clip();

		/// Get the cache object (always return NULL for this reader)
		openshot::CacheMemory* GetCache() override { return &final_cache; };

		/// Determine if reader is open or closed
		bool IsOpen() override { return is_open; };

		/// Get and set the object id that this clip is attached to
		std::string GetAttachedId() const { return parentObjectId; };
		/// Set id of the object id that this clip is attached to
		void SetAttachedId(std::string value) { parentObjectId = value; };

		/// Attach clip to Tracked Object or to another Clip
		void AttachToObject(std::string object_id);

		/// Set the pointer to the trackedObject this clip is attached to
		void SetAttachedObject(std::shared_ptr<openshot::TrackedObjectBase> trackedObject);
		/// Set the pointer to the clip this clip is attached to
		void SetAttachedClip(Clip* clipObject);
		/// Return a pointer to the trackedObject this clip is attached to
		std::shared_ptr<openshot::TrackedObjectBase> GetAttachedObject() const { return parentTrackedObject; };
		/// Return a pointer to the clip this clip is attached to
		Clip* GetAttachedClip() const { return parentClipObject; };

		/// Return the type name of the class
		std::string Name() override { return "Clip"; };

		/// @brief Add an effect to the clip
		/// @param effect Add an effect to the clip. An effect can modify the audio or video of an openshot::Frame.
		void AddEffect(openshot::EffectBase* effect);

		/// Close the internal reader
		void Close() override;

		/// Return the associated ParentClip (if any)
		openshot::Clip* GetParentClip();

		/// Return the associated Parent Tracked Object (if any)
		std::shared_ptr<openshot::TrackedObjectBase> GetParentTrackedObject();

		/// Return the list of effects on the timeline
		std::list<openshot::EffectBase*> Effects() { return effects; };

		/// Look up an effect by ID
		openshot::EffectBase* GetEffect(const std::string& id);

		/// @brief Get an openshot::Frame object for a specific frame number of this clip. The image size and number
		/// of samples match the source reader.
		///
		/// @returns A new openshot::Frame object
		/// @param clip_frame_number The frame number (starting at 1) of the clip
		std::shared_ptr<openshot::Frame> GetFrame(int64_t clip_frame_number) override;

		/// @brief Get an openshot::Frame object for a specific frame number of this clip. The image size and number
		/// of samples match the background_frame passed in and the timeline (if available).
		///
		/// A new openshot::Frame objects is returned, based on a copy from the source image, with all keyframes and clip effects
		/// rendered/rasterized.
		///
		/// @returns The modified openshot::Frame object
		/// @param background_frame The frame object to use as a background canvas (i.e. an existing Timeline openshot::Frame instance)
		/// @param clip_frame_number The frame number (starting at 1) of the clip. The image size and number
		/// of samples match the background_frame passed in and the timeline (if available)
		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> background_frame, int64_t clip_frame_number) override;

		/// @brief Get an openshot::Frame object for a specific frame number of this clip. The image size and number
		/// of samples match the background_frame passed in and the timeline (if available).
		///
		/// A new openshot::Frame objects is returned, based on a copy from the source image, with all keyframes and clip effects
		/// rendered/rasterized.
		///
		/// @returns The modified openshot::Frame object
		/// @param background_frame The frame object to use as a background canvas (i.e. an existing Timeline openshot::Frame instance)
		/// @param clip_frame_number The frame number (starting at 1) of the clip on the timeline. The image size and number
		/// of samples match the timeline.
		/// @param options The openshot::TimelineInfoStruct pointer, with more details about this specific timeline clip,
		/// such as, if it's a top clip. This info is used to apply global transitions and masks, if needed.
		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> background_frame, int64_t clip_frame_number, openshot::TimelineInfoStruct* options);

		/// Open the internal reader
		void Open() override;

		/// @brief Set the current reader
		/// @param new_reader The reader to be used by this clip
		void Reader(openshot::ReaderBase* new_reader);

		/// Get the current reader
		openshot::ReaderBase* Reader();

		// Override End() position (in seconds) of clip (trim end of video)
		float End() const override; ///< Get end position (in seconds) of clip (trim end of video), which can be affected by the time curve.
		void End(float value) override; ///< Set end position (in seconds) of clip (trim end of video)
		openshot::TimelineBase* ParentTimeline() override { return timeline; } ///< Get the associated Timeline pointer (if any)
		void ParentTimeline(openshot::TimelineBase* new_timeline) override; ///< Set associated Timeline pointer

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		std::string PropertiesJSON(int64_t requested_frame) const override;

		/// @brief Remove an effect from the clip
		/// @param effect Remove an effect from the clip.
		void RemoveEffect(openshot::EffectBase* effect);

		// Waveform property
		bool Waveform() { return waveform; } ///< Get the waveform property of this clip
		void Waveform(bool value) { waveform = value; } ///< Set the waveform property of this clip

		// Scale, Location, and Alpha curves
		openshot::Keyframe scale_x; ///< Curve representing the horizontal scaling in percent (0 to 1)
		openshot::Keyframe scale_y; ///< Curve representing the vertical scaling in percent (0 to 1)
		openshot::Keyframe location_x; ///< Curve representing the relative X position in percent based on the gravity (-1 to 1)
		openshot::Keyframe location_y; ///< Curve representing the relative Y position in percent based on the gravity (-1 to 1)
		openshot::Keyframe alpha; ///< Curve representing the alpha (1 to 0)

		// Rotation and Shear curves (origin point (x,y) is adjustable for both rotation and shear)
		openshot::Keyframe rotation; ///< Curve representing the rotation (0 to 360)
		openshot::Keyframe shear_x; ///< Curve representing X shear angle in degrees (-45.0=left, 45.0=right)
		openshot::Keyframe shear_y; ///< Curve representing Y shear angle in degrees (-45.0=down, 45.0=up)
		openshot::Keyframe origin_x; ///< Curve representing X origin point (0.0=0% (left), 1.0=100% (right))
		openshot::Keyframe origin_y; ///< Curve representing Y origin point (0.0=0% (top), 1.0=100% (bottom))

		// Time and Volume curves
		openshot::Keyframe time; ///< Curve representing the frames over time to play (used for speed and direction of video)
		openshot::Keyframe volume; ///< Curve representing the volume (0 to 1)

		/// Curve representing the color of the audio wave form
		openshot::Color wave_color;

		// Perspective curves
		openshot::Keyframe perspective_c1_x; ///< Curves representing X for coordinate 1
		openshot::Keyframe perspective_c1_y; ///< Curves representing Y for coordinate 1
		openshot::Keyframe perspective_c2_x; ///< Curves representing X for coordinate 2
		openshot::Keyframe perspective_c2_y; ///< Curves representing Y for coordinate 2
		openshot::Keyframe perspective_c3_x; ///< Curves representing X for coordinate 3
		openshot::Keyframe perspective_c3_y; ///< Curves representing Y for coordinate 3
		openshot::Keyframe perspective_c4_x; ///< Curves representing X for coordinate 4
		openshot::Keyframe perspective_c4_y; ///< Curves representing Y for coordinate 4

		// Audio channel filter and mappings
		openshot::Keyframe channel_filter; ///< A number representing an audio channel to filter (clears all other channels)
		openshot::Keyframe channel_mapping; ///< A number representing an audio channel to output (only works when filtering a channel)

		// Override has_video and has_audio properties of clip (and their readers)
		openshot::Keyframe has_audio; ///< An optional override to determine if this clip has audio (-1=undefined, 0=no, 1=yes)
		openshot::Keyframe has_video; ///< An optional override to determine if this clip has video (-1=undefined, 0=no, 1=yes)
	};
}  // namespace

#endif  // OPENSHOT_CLIP_H
