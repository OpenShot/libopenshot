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

#include <memory>
#include <map>
#include <string>

namespace openshot
{
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
		bool apply_before_clip; ///< Apply effect before we evaluate the clip's keyframes
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

	protected:
		openshot::ClipBase* clip; ///< Pointer to the parent clip instance (if any)

	public:
		/// Parent effect (which properties will set this effect properties)
		EffectBase* parentEffect;

		/// Map of Tracked Object's by their indices (used by Effects that track objects on clips)
		std::map<int, std::shared_ptr<openshot::TrackedObjectBase> > trackedObjects;

		/// Information about the current effect
		EffectInfoStruct info;

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

		/// Get the order that this effect should be executed.
		int Order() const { return order; }

		/// Set the order that this effect should be executed.
		void Order(int new_order) { order = new_order; }

		virtual ~EffectBase() = default;
	};

}

#endif
