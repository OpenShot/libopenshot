/**
 * @file
 * @brief Source file for ChromaKey class
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

#include "ChromaKey.h"
#include "Exceptions.h"
#if HAVE_BABL
#include <babl/babl.h>
#endif

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
ChromaKey::ChromaKey() : fuzz(5.0), method(0) {
	// Init default color
	color = Color();

	// Init effect properties
	init_effect_details();
}

// Default constructor, which takes an openshot::Color object and a 'fuzz' factor, which
// is used to determine how similar colored pixels are matched. The higher the fuzz, the
// more colors are matched.
ChromaKey::ChromaKey(Color color, Keyframe fuzz) : color(color), fuzz(fuzz), method(0)
{
	// Init effect properties
	init_effect_details();
}

/// New constructor, which takes an openshot::Color object, a 'fuzz' factor, and a numeric
/// keying method, which is used together with the fuzz factor to determine how similar
/// colored pixels are matched. The higher the fuzz, the more colors are matched.
ChromaKey::ChromaKey(Color color, Keyframe fuzz, int method) : color(color), fuzz(fuzz), method(method)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void ChromaKey::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "ChromaKey";
	info.name = "Chroma Key (Greenscreen)";
	info.description = "Replaces the color (or chroma) of the frame with transparency (i.e. keys out the color).";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> ChromaKey::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Determine the current HSL (Hue, Saturation, Lightness) for the Chrome
	int threshold = fuzz.GetInt(frame_number);
	long mask_R = color.red.GetInt(frame_number);
	long mask_G = color.green.GetInt(frame_number);
	long mask_B = color.blue.GetInt(frame_number);

	// Get source image's pixels
	std::shared_ptr<QImage> image = frame->GetImage();
	unsigned char *pixels = (unsigned char *) image->bits();

	int pixelcount = image->width() * image->height();

#if HAVE_BABL
	if (method != CHROMAKEY_METHOD_BASIC)
	{
		static bool need_init = true;


		if (need_init)
		{
			babl_init();
			need_init = false;
		}

		Babl const *rgb = babl_format("RGBA u8");
		Babl const *format = 0;
		Babl const *fish = 0;

		switch(method)
		{
		case CHROMAKEY_METHOD_HSV_H:
		case CHROMAKEY_METHOD_HSV_S:
		case CHROMAKEY_METHOD_HSV_V:
			format = babl_format("HSV float");
			break;

		case CHROMAKEY_METHOD_CIE_LCH_L:
		case CHROMAKEY_METHOD_CIE_LCH_C:
		case CHROMAKEY_METHOD_CIE_LCH_H:
			format = babl_format("CIE LCH(ab) float");
			break;

		case CHROMAKEY_METHOD_CIE_DISTANCE:
			format = babl_format("CIE Lab u8");
			break;
		}

		if (rgb && format && (fish = babl_fish(rgb, format)) != 0)
		{
			int		idx = 0;
			unsigned char	mask_in[4];
			union { float f[4]; unsigned char u[4]; } mask;

			pixelbuf.resize(pixelcount * sizeof(float) * 3);

			float const *pf = (float *) pixelbuf.data();
			unsigned char const *pc = pixelbuf.data();
			int pixel;

			mask_in[0] = mask_R;
			mask_in[1] = mask_G;
			mask_in[2] = mask_B;
			mask_in[3] = 255;
			babl_process(fish, mask_in, &mask, 1);

			babl_process(fish, pixels, pixelbuf.data(), pixelcount);

			switch(method)
			{
			case CHROMAKEY_METHOD_HSV_H:
				for (pixel = 0; pixel < pixelcount; pixel++, pixels += 4, pf += 3)
				{
					float tmp = fabs(pf[0] - mask.f[0]);

					tmp = fabs(pf[0] - mask.f[0]);
					if (tmp > 0.5)
						tmp = 1.0 - tmp;
					if (tmp * 500 <= threshold)
						pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
				}
				break;

			case CHROMAKEY_METHOD_HSV_S:
				for (pixel = 0; pixel < pixelcount; pixel++, pixels += 4, pf += 3)
				{
					if (fabs(pf[1] - mask.f[1]) * 255 <= threshold)
						pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
				}
				break;

			case CHROMAKEY_METHOD_HSV_V:
				for (pixel = 0; pixel < pixelcount; pixel++, pixels += 4, pf += 3)
				{
					if (fabs((pf[2] - mask.f[2]) * 255) <= threshold)
						pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
				}
				break;

			case CHROMAKEY_METHOD_CIE_LCH_L:
				for (pixel = 0; pixel < pixelcount; pixel++, pixels += 4, pf += 3)
				{
					if (fabs(pf[0] - mask.f[0]) <= threshold)
						pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
				}
				break;

			case CHROMAKEY_METHOD_CIE_LCH_C:
				for (pixel = 0; pixel < pixelcount; pixel++, pixels += 4, pf += 3)
				{
					if (fabs(pf[1] - mask.f[1]) <= threshold)
						pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
				}
				break;

			case CHROMAKEY_METHOD_CIE_LCH_H:
				for (pixel = 0; pixel < pixelcount; pixel++, pixels += 4, pf += 3)
				{
					float tmp = fabs(pf[2] - mask.f[2]);

					if (tmp > 180.0)
						tmp = 360.0 - tmp;
					if (tmp <= threshold)
						pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
				}
				break;

			case CHROMAKEY_METHOD_CIE_DISTANCE:
				{
					float KL = 1.0;
					float KC = 1.0;
					float KH = 1.0;
					float pi = 4 * atan(1);

					float L1 = ((float) mask.u[0]) / 2.55;
					float a1 = mask.u[1] - 127;
					float b1 = mask.u[2] - 127;
					float C1 = sqrt(a1 * a1 + b1 * b1);

					for (pixel = 0; pixel < pixelcount; pixel++, pixels += 4, pc += 3)
					{
						float L2 = ((float) pc[0]) / 2.55;
						float a2 = pc[1] - 127;
						float b2 = pc[2] - 127;
						float C2 = sqrt(a2 * a2 + b2 * b2);

						float delta_L_prime = L2 - L1;
						float L_bar = (L1 + L2) / 2;
						float C_bar = (C1 + C2) / 2;

						float a_prime_multiplier = 1 + 0.5 * (1 - sqrt(C_bar / (C_bar + 25)));
						float a1_prime = a1 * a_prime_multiplier;
						float a2_prime = a2 * a_prime_multiplier;

						float C1_prime = sqrt(a1_prime * a1_prime + b1 * b1);
						float C2_prime = sqrt(a2_prime * a2_prime + b2 * b2);
						float C_prime_bar = (C1_prime + C2_prime) / 2;
						float delta_C_prime = C2_prime - C1_prime;

						float h1_prime = atan2(b1, a1_prime) * 180 / pi;
						float h2_prime = atan2(b2, a2_prime) * 180 / pi;

						float delta_h_prime = h2_prime - h1_prime;
						double H_prime_bar = (C1_prime != 0 && C2_prime != 0) ? (h1_prime + h2_prime) / 2 : (h1_prime + h2_prime);

						if (delta_h_prime < -180)
						{
							delta_h_prime += 360;
							if (H_prime_bar < 180)
								H_prime_bar += 180;
							else
								H_prime_bar -= 180;
						}
						else if (delta_h_prime > 180)
						{
							delta_h_prime -= 360;
							if (H_prime_bar < 180)
								H_prime_bar += 180;
							else
								H_prime_bar -= 180;
						}

						float delta_H_prime = 2 * sqrt(C1_prime * C2_prime) * sin(delta_h_prime * pi / 360);

						float T = 1
							- 0.17 * cos((H_prime_bar - 30) * pi / 180)
							+ 0.24 * cos(H_prime_bar * pi / 90)
							+ 0.32 * cos((3 * H_prime_bar + 6) * pi / 180)
							- 0.20 * cos((4 * H_prime_bar - 64) * pi / 180);

						float SL = 1 + 0.015 * square(L_bar - 50) / sqrt(20 + square(L_bar - 50));
						float SC = 1 + 0.045 * C_prime_bar;
						float SH = 1 + 0.015 * C_prime_bar * T;
						float RT = -2 * sqrt(C_prime_bar / (C_prime_bar + 25)) * sin(pi / 3 * exp(-square((H_prime_bar - 275) / 25)));
						float delta_E = sqrt(square(delta_L_prime / KL / SL)
									+ square(delta_C_prime / KC / SC)
									+ square(delta_h_prime / KH / SH)
									+ RT * delta_C_prime / KC / SC * delta_H_prime / KH / SH);
						if (delta_E < threshold)
							pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
					}
				}
				break;
			}

			return frame;
		}
	}
#endif

	// Loop through pixels
	for (int pixel = 0; pixel < pixelcount; pixel++, pixels += 4)
	{
		// Get the RGB values from the pixel
		// Remove the premultiplied alpha values from R,G,B
		float A = float(pixels[3]);
		unsigned char R = (pixels[0] / A) * 255.0;
		unsigned char G = (pixels[1] / A) * 255.0;
		unsigned char B = (pixels[2] / A) * 255.0;

	        // Get distance between mask color and pixel color
		long distance = Color::GetDistance((long)R, (long)G, (long)B, mask_R, mask_G, mask_B);

		if (distance <= threshold) {
			// MATCHED - Make pixel transparent
			// Due to premultiplied alpha, we must also zero out
			// the individual color channels (or else artifacts are left behind)
			pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
		}
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string ChromaKey::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value ChromaKey::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["color"] = color.JsonValue();
	root["fuzz"] = fuzz.JsonValue();
	root["keymethod"] = method;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void ChromaKey::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void ChromaKey::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
	if (!root["fuzz"].isNull())
		fuzz.SetJsonValue(root["fuzz"]);
	if (!root["keymethod"].isNull())
		method = root["keymethod"].asInt();
}

// Get all properties for a specific frame
std::string ChromaKey::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);

	// Keyframes
	root["color"] = add_property_json("Key Color", 0.0, "color", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["fuzz"] = add_property_json("Fuzz", fuzz.GetValue(requested_frame), "float", "", &fuzz, 0, 125, false, requested_frame);
	root["keymethod"] = add_property_json("Key Method", method, "int", "", NULL, 0, CHROMAKEY_METHOD_LAST, false, requested_frame);
	root["keymethod"]["choices"].append(add_property_choice_json("Basic keying", 0, method));
	root["keymethod"]["choices"].append(add_property_choice_json("HSV hue", 1, method));
	root["keymethod"]["choices"].append(add_property_choice_json("HSV saturation", 2, method));
	root["keymethod"]["choices"].append(add_property_choice_json("HSV value", 3, method));
	root["keymethod"]["choices"].append(add_property_choice_json("LCH luminosity", 4, method));
	root["keymethod"]["choices"].append(add_property_choice_json("LCH chroma", 5, method));
	root["keymethod"]["choices"].append(add_property_choice_json("LCH hue", 6, method));
	root["keymethod"]["choices"].append(add_property_choice_json("CIE Distance", 7, method));

	// Set the parent effect which properties this effect will inherit
	root["parent_effect_id"] = add_property_json("Parent", 0.0, "string", info.parent_effect_id, NULL, -1, -1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
