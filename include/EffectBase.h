/**
 * @file
 * @brief Header file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_EFFECT_BASE_H
#define OPENSHOT_EFFECT_BASE_H

#include <iostream>
#include <iomanip>
#include <tr1/memory>
#include "ClipBase.h"
#include "Frame.h"

using namespace std;

namespace openshot
{
	/**
	 * @brief This struct contains info about an effect, such as the name, video or audio effect, etc...
	 *
	 * Each derived class of EffectBase is responsible for updating this struct to reflect accurate information
	 * about the underlying effect. Derived classes of EffectBase should call the InitEffectInfo() method to initialize the
	 * default values of this struct.
	 */
	struct EffectInfo
	{
		string name; ///< The name of the effect
		string description; ///< The description of this effect and what it does
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
	public:

		/// Information about the current effect
		EffectInfo info;

		/// Display effect information in the standard output stream (stdout)
		void DisplayInfo();

		/// @brief This method is required for all derived classes of EffectBase, and returns a
		/// modified openshot::Frame object
		///
		/// The frame object is passed into this method, and a frame_number is passed in which
		/// tells the effect which settings to use from it's keyframes (starting at 1).
		///
		/// @returns The modified openshot::Frame object
		/// @param frame The frame object that needs the effect applied to it
		/// @param frame_number The frame number (starting at 1) of the effect on the timeline.
		virtual tr1::shared_ptr<Frame> GetFrame(tr1::shared_ptr<Frame> frame, int frame_number) = 0;

		/// Initialize the values of the EffectInfo struct.  It is important for derived classes to call
		/// this method, or the EffectInfo struct values will not be initialized.
		void InitEffectInfo();

		/// Get the order that this effect should be executed.
		int Order() { return order; }

		/// Set the order that this effect should be executed.
		void Order(int new_order) { order = new_order; }
	};

}

#endif
