/**
 * @file
 * @brief Header file for Timeline class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_TIMELINE_H
#define OPENSHOT_TIMELINE_H

#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtCore/QRegularExpression>

#include "TimelineBase.h"
#include "ReaderBase.h"

#include "Color.h"
#include "Clip.h"
#include "EffectBase.h"
#include "Fraction.h"
#include "Frame.h"
#include "KeyFrame.h"
#ifdef USE_OPENCV
#include "TrackedObjectBBox.h"
#endif
#include "TrackedObjectBase.h"



namespace openshot {

	// Forward decls
	class FrameMapper;
	class CacheBase;

	/// Comparison method for sorting clip pointers (by Layer and then Position). Clips are sorted
	/// from lowest layer to top layer (since that is the sequence they need to be combined), and then
	/// by position (left to right).
	struct CompareClips{
		bool operator()( openshot::Clip* lhs, openshot::Clip* rhs){
			if( lhs->Layer() < rhs->Layer() ) return true;
			if( lhs->Layer() == rhs->Layer() && lhs->Position() <= rhs->Position() ) return true;
			return false;
	}};

	/// Comparison method for sorting effect pointers (by Position, Layer, and Order). Effects are sorted
	/// from lowest layer to top layer (since that is sequence clips are combined), and then by
	/// position, and then by effect order.
	struct CompareEffects{
		bool operator()( openshot::EffectBase* lhs, openshot::EffectBase* rhs){
			if( lhs->Layer() < rhs->Layer() ) return true;
			if( lhs->Layer() == rhs->Layer() && lhs->Position() < rhs->Position() ) return true;
			if( lhs->Layer() == rhs->Layer() && lhs->Position() == rhs->Position() && lhs->Order() > rhs->Order() ) return true;
			return false;
	}};

	/// Comparison method for finding the far end of the timeline, by locating
	/// the Clip with the highest end-frame number using std::max_element
	struct CompareClipEndFrames {
		bool operator()(const openshot::Clip* lhs, const openshot::Clip* rhs) {
			return (lhs->Position() + lhs->Duration())
			       <= (rhs->Position() + rhs->Duration());
	}};

	/// Like CompareClipEndFrames, but for effects
	struct CompareEffectEndFrames {
		bool operator()(const openshot::EffectBase* lhs, const openshot::EffectBase* rhs) {
			return (lhs->Position() + lhs->Duration())
				<= (rhs->Position() + rhs->Duration());
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
	 * \image html Timeline_Layers.png
	 *
	 * The <b>following graphic</b> displays how the playhead determines which frames to combine and layer.
	 * \image html Playhead.png
	 *
	 * Lets take a look at what the code looks like:
	 * @code
	 * // Create a Timeline
	 * Timeline t(1280, // width
	 *            720, // height
	 *            Fraction(25,1), // framerate
	 *            44100, // sample rate
	 *            2 // channels
	 *            ChannelLayout::LAYOUT_STEREO,
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
	class Timeline : public openshot::TimelineBase, public openshot::ReaderBase {
	private:
		bool is_open; ///<Is Timeline Open?
		bool auto_map_clips; ///< Auto map framerates and sample rates to all clips
		std::list<openshot::Clip*> clips; ///<List of clips on this timeline
		std::list<openshot::Clip*> closing_clips; ///<List of clips that need to be closed
		std::map<openshot::Clip*, openshot::Clip*> open_clips; ///<List of 'opened' clips on this timeline
		std::list<openshot::EffectBase*> effects; ///<List of clips on this timeline
		openshot::CacheBase *final_cache; ///<Final cache of timeline frames
		std::set<openshot::FrameMapper*> allocated_frame_mappers; ///< all the frame mappers we allocated and must free
		bool managed_cache; ///< Does this timeline instance manage the cache object
		std::string path; ///< Optional path of loaded UTF-8 OpenShot JSON project file
		int max_concurrent_frames; ///< Max concurrent frames to process at one time

		std::map<std::string, std::shared_ptr<openshot::TrackedObjectBase>> tracked_objects; ///< map of TrackedObjectBBoxes and their IDs

		/// Process a new layer of video or audio
		void add_layer(std::shared_ptr<openshot::Frame> new_frame, openshot::Clip* source_clip, int64_t clip_frame_number, bool is_top_clip, float max_volume);

		/// Apply a FrameMapper to a clip which matches the settings of this timeline
		void apply_mapper_to_clip(openshot::Clip* clip);

		// Apply JSON Diffs to various objects contained in this timeline
		void apply_json_to_clips(Json::Value change); ///<Apply JSON diff to clips
		void apply_json_to_effects(Json::Value change); ///< Apply JSON diff to effects
		void apply_json_to_effects(Json::Value change, openshot::EffectBase* existing_effect); ///<Apply JSON diff to a specific effect
		void apply_json_to_timeline(Json::Value change); ///<Apply JSON diff to timeline properties

		/// Calculate time of a frame number, based on a framerate
		double calculate_time(int64_t number, openshot::Fraction rate);

		/// Find intersecting (or non-intersecting) openshot::Clip objects
		///
		/// @returns A list of openshot::Clip objects
		/// @param requested_frame The frame number that is requested.
		/// @param number_of_frames The number of frames to check
		/// @param include Include or Exclude intersecting clips
		std::vector<openshot::Clip*> find_intersecting_clips(int64_t requested_frame, int number_of_frames, bool include);

		/// Get a clip's frame or generate a blank frame
		std::shared_ptr<openshot::Frame> GetOrCreateFrame(std::shared_ptr<Frame> background_frame, openshot::Clip* clip, int64_t number, openshot::TimelineInfoStruct* options);

		/// Compare 2 floating point numbers for equality
		bool isEqual(double a, double b);

		/// Sort clips by position on the timeline
		void sort_clips();

		/// Sort effects by position on the timeline
		void sort_effects();

		/// Update the list of 'opened' clips
		void update_open_clips(openshot::Clip *clip, bool does_clip_intersect);

	public:

		/// @brief Constructor for the timeline (which configures the default frame properties)
		/// @param width The image width of generated openshot::Frame objects
		/// @param height The image height of generated openshot::Frame objects
		/// @param fps The frame rate of the generated video
		/// @param sample_rate The audio sample rate
		/// @param channels The number of audio channels
		/// @param channel_layout The channel layout (i.e. mono, stereo, 3 point surround, etc...)
		Timeline(int width, int height, openshot::Fraction fps, int sample_rate, int channels, openshot::ChannelLayout channel_layout);

		/// @brief Constructor which takes a ReaderInfo struct to configure parameters
		/// @param info The reader parameters to configure the new timeline with
		Timeline(ReaderInfo info);

		/// @brief Project-file constructor for the timeline
		///
		/// Loads a JSON structure from a file path, and
		/// initializes the timeline described within.
		///
		/// @param projectPath The path of the UTF-8 *.osp project file (JSON contents). Contents will be loaded automatically.
		/// @param convert_absolute_paths Should all paths be converted to absolute paths (relative to the location of projectPath)
		Timeline(const std::string& projectPath, bool convert_absolute_paths);

        virtual ~Timeline();

		/// Add to the tracked_objects map a pointer to a tracked object (TrackedObjectBBox)
		void AddTrackedObject(std::shared_ptr<openshot::TrackedObjectBase> trackedObject);
		/// Return tracked object pointer by it's id
		std::shared_ptr<openshot::TrackedObjectBase> GetTrackedObject(std::string id) const;
		/// Return the ID's of the tracked objects as a list of strings
		std::list<std::string> GetTrackedObjectsIds() const;
		/// Return the trackedObject's properties as a JSON string
        #ifdef USE_OPENCV
		std::string GetTrackedObjectValues(std::string id, int64_t frame_number) const;
        #endif

		/// @brief Add an openshot::Clip to the timeline
		/// @param clip Add an openshot::Clip to the timeline. A clip can contain any type of Reader.
		void AddClip(openshot::Clip* clip);

		/// @brief Add an effect to the timeline
		/// @param effect Add an effect to the timeline. An effect can modify the audio or video of an openshot::Frame.
		void AddEffect(openshot::EffectBase* effect);

        /// Apply global/timeline effects to the source frame (if any)
        std::shared_ptr<openshot::Frame> apply_effects(std::shared_ptr<openshot::Frame> frame, int64_t timeline_frame_number, int layer);

		/// Apply the timeline's framerate and samplerate to all clips
		void ApplyMapperToClips();

		/// Determine if clips are automatically mapped to the timeline's framerate and samplerate
		bool AutoMapClips() { return auto_map_clips; };

		/// @brief Automatically map all clips to the timeline's framerate and samplerate
		void AutoMapClips(bool auto_map) { auto_map_clips = auto_map; };

        /// Clear all cache for this timeline instance, and all clips, mappers, and readers under it
        void ClearAllCache();

		/// Return a list of clips on the timeline
		std::list<openshot::Clip*> Clips() override { return clips; };

		/// Look up a single clip by ID
		openshot::Clip* GetClip(const std::string& id);

		/// Look up a clip effect by ID
		openshot::EffectBase* GetClipEffect(const std::string& id);

		/// Look up a timeline effect by ID
		openshot::EffectBase* GetEffect(const std::string& id);

		/// Look up the end time of the latest timeline element
		double GetMaxTime();
		/// Look up the end frame number of the latest element on the timeline
		int64_t GetMaxFrame();

		/// Close the timeline reader (and any resources it was consuming)
		void Close() override;

		/// Return the list of effects on the timeline
		std::list<openshot::EffectBase*> Effects() { return effects; };

		/// Return the list of effects on all clips
		std::list<openshot::EffectBase*> ClipEffects() const;

		/// Get the cache object used by this reader
		openshot::CacheBase* GetCache() override { return final_cache; };

		/// Set the cache object used by this reader. You must now manage the lifecycle
		/// of this cache object though (Timeline will not delete it for you).
		void SetCache(openshot::CacheBase* new_cache);

		/// Get an openshot::Frame object for a specific frame number of this timeline.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t requested_frame) override;

		// Curves for the viewport
		openshot::Keyframe viewport_scale; ///<Curve representing the scale of the viewport (0 to 100)
		openshot::Keyframe viewport_x; ///<Curve representing the x coordinate for the viewport
		openshot::Keyframe viewport_y; ///<Curve representing the y coordinate for the viewport

		// Background color
		openshot::Color color; ///<Background color of timeline canvas

		/// Determine if reader is open or closed
		bool IsOpen() override { return is_open; };

		/// Return the type name of the class
		std::string Name() override { return "Timeline"; };

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Set Max Image Size (used for performance optimization). Convenience function for setting
		/// Settings::Instance()->MAX_WIDTH and Settings::Instance()->MAX_HEIGHT.
		void SetMaxSize(int width, int height);

		/// @brief Apply a special formatted JSON object, which represents a change to the timeline (add, update, delete)
		/// This is primarily designed to keep the timeline (and its child objects... such as clips and effects) in sync
		/// with another application... such as OpenShot Video Editor (http://www.openshot.org).
		/// @param value A JSON string containing a key, value, and type of change.
		void ApplyJsonDiff(std::string value);

		/// Open the reader (and start consuming resources)
		void Open() override;

		/// @brief Remove an openshot::Clip from the timeline
		/// @param clip Remove an openshot::Clip from the timeline.
		void RemoveClip(openshot::Clip* clip);

		/// @brief Remove an effect from the timeline
		/// @param effect Remove an effect from the timeline.
		void RemoveEffect(openshot::EffectBase* effect);
	};

}

#endif // OPENSHOT_TIMELINE_H
