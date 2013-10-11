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
		mask = tr1::shared_ptr<Magick::Image>(new Magick::Image(path));
		mask->type(Magick::GrayscaleType); // convert to grayscale

		// Remove transparency support (so mask will sub color brightness for alpha)
		mask->matte(false); // This is required for the composite operator to copy the brightness of each pixel into the alpha channel

	}
	catch (Magick::Exception e) {
		// raise exception
		throw InvalidFile("File could not be opened.", path);
	}
}

// Set brightness and contrast (brightness between -100 and 100)
void Wipe::set_brightness_and_contrast(tr1::shared_ptr<Magick::Image> image, float brightness, float contrast)
{
	// Determine if white or black image is needed
	if (brightness >= -100.0 and brightness <= 0.0)
	{
		// Make mask darker
		double black_alpha = abs(brightness) / 100.0;
		tr1::shared_ptr<Magick::Image> black = tr1::shared_ptr<Magick::Image>(new Magick::Image(mask->size(), Magick::Color("Black")));
		black->matte(true);
		black->quantumOperator(Magick::OpacityChannel, Magick::MultiplyEvaluateOperator, black_alpha);
		image->composite(*black.get(), 0, 0, Magick::OverCompositeOp);

	}
	else if (brightness > 0.0 and brightness <= 100.0)
	{
		// Make mask whiter
		double white_alpha = brightness / 100.0;
		tr1::shared_ptr<Magick::Image> white = tr1::shared_ptr<Magick::Image>(new Magick::Image(mask->size(), Magick::Color("White")));
		white->matte(true);
		white->quantumOperator(Magick::OpacityChannel, Magick::MultiplyEvaluateOperator, white_alpha);
		image->composite(*white.get(), 0, 0, Magick::OverCompositeOp);
	}

	// Set Contrast
	image->sigmoidalContrast(true, contrast);

}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
tr1::shared_ptr<Frame> Wipe::GetFrame(tr1::shared_ptr<Frame> frame, int frame_number)
{
	// Resize mask to match this frame size (if different)
	if (frame->GetImage()->size() != mask->size())
	{
		Magick::Geometry new_size(frame->GetImage()->size().width(), frame->GetImage()->size().height());
		new_size.aspect(true);
		mask->resize(new_size);
	}

	// Make a copy of the resized image (since we will be modifying the brightness and contrast)
	tr1::shared_ptr<Magick::Image> image = tr1::shared_ptr<Magick::Image>(new Magick::Image(*mask.get()));

	// Set the brightness of the mask (from a user-defined curve)
	set_brightness_and_contrast(image, brightness.GetValue(frame_number), contrast.GetValue(frame_number));

	// Composite the alpha channel of our mask onto our frame's alpha channel
	tr1::shared_ptr<Magick::Image> alpha_image = tr1::shared_ptr<Magick::Image>(new Magick::Image(mask->size(), Magick::Color("White")));
	alpha_image->backgroundColor(Magick::Color("none"));
	alpha_image->matte(true);
	alpha_image->composite(*image.get(), 0, 0, Magick::CopyOpacityCompositeOp);

	// Add 2 alpha channels together (i.e. maintain the original alpha while adding more)
	// Prepare to update image
	frame->GetImage()->modifyImage();

	// Loop through each row
	for (int row = 0; row < frame->GetImage()->size().height(); row++)
	{
		const Magick::PixelPacket* original_pixels = frame->GetImage()->getConstPixels(0, row, frame->GetImage()->columns(), 1);
		const Magick::PixelPacket* alpha_pixels = alpha_image->getConstPixels(0, row, alpha_image->columns(), 1);

		// Loop through each column
		for (int col = 0; col < frame->GetImage()->size().width(); col++)
		{
			// Calculate new opacity value (and prevent overflow)
			int new_alpha = original_pixels[col].opacity + alpha_pixels[col].opacity;
			if (new_alpha > 65535.0)
				new_alpha = 65535.0;

			// Set the new opacity
			frame->GetImage()->pixelColor(col, row, Magick::Color(original_pixels[col].red, original_pixels[col].green, original_pixels[col].blue, new_alpha));
		}
	}
	// Transfer image cache pixels to the image
	frame->GetImage()->syncPixels();



	// Copy the combined alpha channel back to the frame
	//frame->GetImage()->composite(*combined_image.get(), 0, 0, Magick::CopyOpacityCompositeOp);


	// return the modified frame
	return frame;
}
