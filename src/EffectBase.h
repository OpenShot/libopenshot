/**
 * @file
 * @brief Header file for EffectBase class
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

#ifndef OPENSHOT_EFFECT_BASE_H
#define OPENSHOT_EFFECT_BASE_H

#include <iostream>
#include <iomanip>
#include <memory>
#include "ClipBase.h"
#include "Json.h"
#include "Frame.h"

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
		bool has_video;	///< Determines if this effect manipulates the image of a frame
		bool has_audio;	///< Determines if this effect manipulates the audio of a frame
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

		/// Information about the current effect
		EffectInfoStruct info;

		/// Display effect information in the standard output stream (stdout)
		void DisplayInfo();

		/// Constrain a color value from 0 to 255
		int constrain(int color_value);

		/// Initialize the values of the EffectInfo struct.  It is important for derived classes to call
		/// this method, or the EffectInfo struct values will not be initialized.
		void InitEffectInfo();

		/// Parent clip object of this effect (which can be unparented and NULL)
		openshot::ClipBase* ParentClip();

		/// Set parent clip object of this effect
		void ParentClip(openshot::ClipBase* new_clip);

		/// Get and Set JSON methods
		virtual std::string Json() const = 0; ///< Generate JSON string of this object
		virtual void SetJson(const std::string value) = 0; ///< Load JSON string into this object
		virtual Json::Value JsonValue() const = 0; ///< Generate Json::Value for this object
		virtual void SetJsonValue(const Json::Value root) = 0; ///< Load Json::Value into this object
		Json::Value JsonInfo() const; ///< Generate JSON object of meta data / info

		/// Get the order that this effect should be executed.
		int Order() const { return order; }

		/// Set the order that this effect should be executed.
		void Order(int new_order) { order = new_order; }
		virtual ~EffectBase() = default;
	};

}

#endif
