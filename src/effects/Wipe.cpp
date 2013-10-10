/**
 * @file
 * @brief Source file for Wipe class
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

#include "../include/effects/Wipe.h"

using namespace openshot;

// Default constructor, which takes an openshot::Color object and a 'fuzz' factor, which
// is used to determine how similar colored pixels are matched. The higher the fuzz, the
// more colors are matched.
Wipe::Wipe(string mask_path, Keyframe mask_brightness, Keyframe mask_contrast) throw(InvalidFile) :
		path(mask_path), brightness(mask_brightness), contrast(mask_contrast)
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.name = "Wipe (Transition)";
	info.description = "Uses a grayscale mask image file to gradually wipe / transition between 2 images.";
	info.has_audio = false;
	info.has_video = true;

	// Attempt to open mask file
	try
	{
		// load image
		image = tr1::shared_ptr<Magick::Image>(new Magick::Image(path));
		image->type(Magick::GrayscaleType); // convert to grayscale

		// Give image a transparent background color
		//image->backgroundColor(Magick::Color("none"));
	}
	catch (Magick::Exception e) {
		// raise exception
		throw InvalidFile("File could not be opened.", path);
	}
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
tr1::shared_ptr<Frame> Wipe::GetFrame(tr1::shared_ptr<Frame> frame, int frame_number)
{
	// Resize mask to match this frame size (if different)
	if (frame->GetImage()->size() != image->size())
		image->resize(frame->GetImage()->size());

	// Set the brightness of the mask (from a user-defined curve)
	image->modulate(brightness.GetValue(frame_number), 100.0, 100.0);

	// Set the contrast of the mask (from a cuser-defined curve)
	image->contrast(contrast.GetInt(frame_number));

	// Composite the alpha channel of our mask onto our frame's alpha channel
	frame->GetImage()->composite(*image.get(), 0, 0, Magick::CopyOpacityCompositeOp);

	// return the modified frame
	return frame;
}
