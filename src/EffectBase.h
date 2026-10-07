/**
 * @file
 * @brief Header file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_EFFECT_BASE_H
#define OPENSHOT_EFFECT_BASE_H

#include "ClipBase.h"

#include "Json.h"
#include "TrackedObjectBase.h"

#include <QImage>
#include <memory>
#include <map>
#include <string>

namespace openshot
{
	class ReaderBase;

	/**
	 * @brief This struct contains info about an effect, such as the name, video or audio effect, etc...
	 *
	 * Each derived class of EffectBase is responsible for updating this struct to reflect accurate information
	 * about the underlying effect. Derived classes of EffectBase should call the InitEffectInfo() method to initialize the
	 * default values of this struct.
	 */
	struct EffectInfoStruct
	{
		std::string class_name; ///< The class name of the effect
		std::string name; ///< The name of the effect
		std::string description; ///< The description of this effect and what it does
		std::string parent_effect_id; ///< Id of the parent effect (if there is one)
		bool has_video;	///< Determines if this effect manipulates the image of a frame
		bool has_audio;	///< Determines if this effect manipulates the audio of a frame
		bool has_tracked_object; ///< Determines if this effect track objects through the clip
		bool apply_before_clip; ///< Apply effect to source before we evaluate the clip's keyframes
	};

	/**
	 * @brief This abstract class is the base class, used by all effects in libopenshot.
	 *
	 * Effects are types of classes that manipulate the image or audio data of an openshot::Frame object.
	 * The only requirements for an 'effect', is to derive from this base class, implement the Apply()
	 * method, and call the InitEffectInfo() method.
	 */
	class EffectBase : public ClipBase
	{
	private:
		int order; ///< The order to evaluate this effect. Effects are processed in this order (when more than one overlap).
		ReaderBase* mask_reader = nullptr; ///< Optional common reader-based mask source.
		std::string mask_source_id; ///< Optional effect ID that provides a generated mask source.
		std::shared_ptr<QImage> cached_single_mask_image; ///< Cached scaled mask for still-image mask sources.
		int cached_single_mask_width = 0; ///< Cached mask width.
		int cached_single_mask_height = 0; ///< Cached mask height.

		/// Build or refresh a mask image that matches target_image dimensions.
		std::shared_ptr<QImage> GetMaskImage(std::shared_ptr<QImage> target_image, int64_t frame_number);

		/// Blend original and effected images using mask values.
		void BlendWithMask(std::shared_ptr<QImage> original_image, std::shared_ptr<QImage> effected_image,
					   std::shared_ptr<QImage> mask_image) const;

		/// Find the effect named by mask_source_id.
		EffectBase* ResolveMaskSourceEffect();

	protected:
		openshot::ClipBase* clip; ///< Pointer to the parent clip instance (if any)

		/// Create a reader instance from reader JSON.
		ReaderBase* CreateReaderFromJson(const Json::Value& reader_json) const;

		/// Convert an effect frame number to a mask source frame number.
		int64_t MapMaskFrameNumber(int64_t frame_number);

		/// Determine host FPS used to convert timeline frames to mask source FPS.
		double ResolveMaskHostFps();

		/// Determine mask source duration in seconds.
		double ResolveMaskSourceDuration() const;

		/// Resolve a cached/scaled mask image for the target frame dimensions.
		std::shared_ptr<QImage> ResolveMaskImage(std::shared_ptr<QImage> target_image, int64_t frame_number) {
			return GetMaskImage(target_image, frame_number);
		}

		/// Optional override for effects that need custom mask behavior.
		virtual bool UseCustomMaskBlend(int64_t frame_number) const { return false; }

		/// Optional override for effects with custom mask implementation.
		virtual void ApplyCustomMaskBlend(std::shared_ptr<QImage> original_image, std::shared_ptr<QImage> effected_image,
								  std::shared_ptr<QImage> mask_image, int64_t frame_number) const {}

		/// Optional override for effects that apply mask processing inside GetFrame().
		virtual bool HandlesMaskInternally() const { return false; }

	public:
		/// Parent effect (which properties will set this effect properties)
		EffectBase* parentEffect;

		/// Map of Tracked Object's by their indices (used by Effects that track objects on clips)
		std::map<int, std::shared_ptr<openshot::TrackedObjectBase> > trackedObjects;

		/// Information about the current effect
		EffectInfoStruct info;
		bool mask_invert = false; ///< Invert grayscale mask values before blending.

		enum MaskTimeMode {
			MASK_TIME_TIMELINE = 0,
			MASK_TIME_SOURCE_FPS = 1
		};

		enum MaskLoopMode {
			MASK_LOOP_PLAY_ONCE = 0,
			MASK_LOOP_REPEAT = 1,
			MASK_LOOP_PING_PONG = 2
		};

		int mask_time_mode = MASK_TIME_SOURCE_FPS; ///< How effect frames map to mask source frames.
		int mask_loop_mode = MASK_LOOP_PLAY_ONCE; ///< Behavior when mask range reaches the end.

		/// Display effect information in the standard output stream (stdout)
		void DisplayInfo(std::ostream* out=&std::cout);

		/// Constrain a color value from 0 to 255
		int constrain(int color_value);

		/// Initialize the values of the EffectInfo struct.  It is important for derived classes to call
		/// this method, or the EffectInfo struct values will not be initialized.
		void InitEffectInfo();

		/// Parent clip object of this effect (which can be unparented and NULL)
		openshot::ClipBase* ParentClip();

		/// Set parent clip object of this effect
		void ParentClip(openshot::ClipBase* new_clip);

		/// Set the parent effect from which this properties will be set to
		void SetParentEffect(std::string parentEffect_id);

		/// Return the ID of this effect's parent clip
		std::string ParentClipId() const;

		/// Get the indexes and IDs of all visible objects in the given frame
		virtual std::string GetVisibleObjects(int64_t frame_number) const {return {}; };

		/// Generate a black/white mask from tracked object data.
		virtual std::shared_ptr<QImage> TrackedObjectMask(std::shared_ptr<QImage> target_image, int64_t frame_number) const;

		// Get and Set JSON methods
		virtual std::string Json() const; ///< Generate JSON string of this object
		virtual void SetJson(const std::string value); ///< Load JSON string into this object
		virtual Json::Value JsonValue() const; ///< Generate Json::Value for this object
		virtual void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object

		virtual std::string Json(int64_t requested_frame) const{
			return "";
		};
		virtual void SetJson(int64_t requested_frame, const std::string value) {
			return;
		};

		/// Generate JSON object of meta data / info
		Json::Value JsonInfo() const; 

		/// Generate JSON object of base properties (recommended to be used by all effects)
		Json::Value BasePropertiesJSON(int64_t requested_frame) const;

		/// Apply effect processing with common mask support (if enabled).
		std::shared_ptr<openshot::Frame> ProcessFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number);

		/// Get the common mask reader.
		ReaderBase* MaskReader() { return mask_reader; }
		const ReaderBase* MaskReader() const { return mask_reader; }

		/// Set or replace the common mask reader.
		void MaskReader(ReaderBase* new_reader);

		/// Get/Set effect ID used as a generated mask source.
		std::string MaskSourceId() const { return mask_source_id; }
		void MaskSourceId(const std::string& new_mask_source_id);

		/// Get the order that this effect should be executed.
		int Order() const { return order; }

		/// Set the order that this effect should be executed.
		void Order(int new_order) { order = new_order; }

		virtual ~EffectBase();
	};

}

#endif
