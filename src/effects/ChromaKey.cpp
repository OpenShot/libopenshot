/**
 * @file
 * @brief Source file for ChromaKey class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Troy Rollo <troy@kawseq.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ChromaKey.h"
#include "Exceptions.h"
#if USE_BABL
#include <babl/babl.h>
#endif
#include <vector>
#include <cmath>

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
ChromaKey::ChromaKey() : fuzz(5.0), halo(0), method(CHROMAKEY_BASIC) {
	// Init default color
	color = Color();

	// Init effect properties
	init_effect_details();
}

// Standard constructor, which takes an openshot::Color object, a 'fuzz' factor,
// an optional halo distance and an optional keying method.
ChromaKey::ChromaKey(Color color, Keyframe fuzz, Keyframe halo, ChromaKeyMethod method) :
	color(color), fuzz(fuzz), halo(halo), method(method)
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
//
// Because a frame's QImage is always in Format_RGB8888_Premultiplied, we do not
// need to muck about with QRgb and its helper functions, qRed, QGreen, QBlue and
// qAlpha, and indeed doing so will get wrong results on almost every platform
// when we operate on the pixel buffers instead of calling the pixel methods in
// QImage. QRgb is always in the form 0xAARRGGBB, but treating the pixel buffer
// as an array of QRgb will yield values of the form 0xAABBGGRR on little endian
// systems and 0xRRGGBBAA on big endian systems.
//
// We need to operate on the pixel buffers here because doing this all pixel by
// pixel is be horribly slow, especially with keying methods other than basic.
// The babl conversion functions are very slow if iterating over pixels and every
// effort should be made to do babl conversions in blocks of as many pixels as
// can be done at once.
//
// The default keying method tries to ascertain the original pixel color by
// dividing the red, green and blue channels by the alpha (and multiplying by
// 255). The other methods do not do this for several reasons:
//
//   1. The calculation will not necessarily return the original value, because
//      the premultiplication of alpha using unsigned 8 bit integers loses
//      accuracy at the least significant bit. Even an alpha of 0xfe means that
//      we are left with only 255 values to start with and cannot regain the full
//      256 values that could have been in the input. At an alpha of 0x7f the
//      entire least significant bit has been lost, and at an alpho of 0x3f the
//      two entire least significant bits have been lost. Chroma keying is very
//      sensitive to these losses of precision so if the alpha has been applied
//      already at anything other than 0xff and 0x00, we are already screwed and
//      this calculation will not help.
//
//   2. The calculation used for the default method seems to be wrong anyway as
//      it always rounds down rather than to the nearest whole number.
//
//   3. As mentioned above, babl conversion functions are very slow when iterating
//      over individual pixels. We would have to convert the entire input buffer
//      in one go to avoid this. It just does not seem worth it given the loss
//      of accuracy we already have.
//
//   4. It is difficult to see how it could make sense to apply chroma keying
//      after other non-chroma-key effects. The purpose is to remove an unwanted
//      background in the input stream, rather than removing some calculated
//      value that is the output of another effect.
std::shared_ptr<openshot::Frame> ChromaKey::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	int threshold = fuzz.GetInt(frame_number);
	int halothreshold = halo.GetInt(frame_number);
	long mask_R = color.red.GetInt(frame_number);
	long mask_G = color.green.GetInt(frame_number);
	long mask_B = color.blue.GetInt(frame_number);

	// Get source image
	std::shared_ptr<QImage> image = frame->GetImage();

	int width = image->width();
	int height = image->height();

	int pixelcount = width * height;

#if USE_BABL
	if (method > CHROMAKEY_BASIC && method <= CHROMAKEY_LAST_METHOD)
	{
		static bool need_init = true;

		if (need_init)
		{
			babl_init();
			need_init = false;
		}

		Babl const *rgb = babl_format("R'G'B'A u8");
		Babl const *format = 0;
		Babl const *fish = 0;
		std::vector<unsigned char> pixelbuf;
		int rowwidth = 0;

		switch(method)
		{
		case CHROMAKEY_HSVL_H:
		case CHROMAKEY_HSV_S:
		case CHROMAKEY_HSV_V:
			format = babl_format("HSV float");
			rowwidth = width * sizeof(float) * 3;
			break;

		case CHROMAKEY_HSL_S:
		case CHROMAKEY_HSL_L:
			format = babl_format("HSL float");
			rowwidth = width * sizeof(float) * 3;
			break;

		case CHROMAKEY_CIE_LCH_L:
		case CHROMAKEY_CIE_LCH_C:
		case CHROMAKEY_CIE_LCH_H:
			format = babl_format("CIE LCH(ab) float");
			rowwidth = width * sizeof(float) * 3;
			break;

		case CHROMAKEY_CIE_DISTANCE:
			format = babl_format("CIE Lab u8");
			rowwidth = width * 3;
			break;

		case CHROMAKEY_YCBCR:
			format = babl_format("Y'CbCr u8");
			rowwidth = width * 3;
			break;

		case CHROMAKEY_BASIC:
			break;
		}

		pixelbuf.resize(rowwidth * height);

		if (rgb && format && (fish = babl_fish(rgb, format)) != 0)
		{
			int		idx = 0;
			unsigned char	mask_in[4];
			union { float f[4]; unsigned char u[4]; } mask;
			float const *pf = (float *) pixelbuf.data();
			unsigned char const *pc = pixelbuf.data();

			mask_in[0] = mask_R;
			mask_in[1] = mask_G;
			mask_in[2] = mask_B;
			mask_in[3] = 255;
			babl_process(fish, mask_in, &mask, 1);

			if (0) //image->bytesPerLine() == width * 4)
			{
				// Because babl_process is expensive to call, but efficient
				// with long sequences of pixels, attempt to convert the
				// entire buffer at once if we can
				babl_process(fish, image->bits(), pixelbuf.data(), pixelcount);
			}
			else
			{
				unsigned char *rowdata = pixelbuf.data();

				for (int y = 0; y < height; ++y, rowdata += rowwidth)
					babl_process(fish, image->scanLine(y), rowdata, width);
			}

			switch(method)
			{
			case CHROMAKEY_HSVL_H:
				for (int y = 0; y < height; ++y)
				{
					unsigned char *pixel = image->scanLine(y);

					for (int x = 0; x < width; ++x, pixel += 4, pf += 3)
					{
						float tmp = fabs(pf[0] - mask.f[0]);

						if (tmp > 0.5)
							tmp = 1.0 - tmp;
						tmp *= 500;
						if (tmp <= threshold)
						{
							pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
						}
						else if (tmp <= threshold + halothreshold)
						{
							float alphamult = (tmp - threshold) / halothreshold;

							pixel[0] *= alphamult;
							pixel[1] *= alphamult;
							pixel[2] *= alphamult;
							pixel[3] *= alphamult;
						}
					}
				}
				break;

			case CHROMAKEY_HSV_S:
			case CHROMAKEY_HSL_S:
				for (int y = 0; y < height; ++y)
				{
					unsigned char *pixel = image->scanLine(y);

					for (int x = 0; x < width; ++x, pixel += 4, pf += 3)
					{
						float tmp = fabs(pf[1] - mask.f[1]) * 255;

						if (tmp <= threshold)
						{
							pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
						}
						else if (tmp <= threshold + halothreshold)
						{
							float alphamult = (tmp - threshold) / halothreshold;

							pixel[0] *= alphamult;
							pixel[1] *= alphamult;
							pixel[2] *= alphamult;
							pixel[3] *= alphamult;
						}
					}
				}
				break;

			case CHROMAKEY_HSV_V:
			case CHROMAKEY_HSL_L:
				for (int y = 0; y < height; ++y)
				{
					unsigned char *pixel = image->scanLine(y);

					for (int x = 0; x < width; ++x, pixel += 4, pf += 3)
					{
						float tmp = fabs(pf[2] - mask.f[2]) * 255;

						if (tmp <= threshold)
						{
							pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
						}
						else if (tmp <= threshold + halothreshold)
						{
							float alphamult = (tmp - threshold) / halothreshold;

							pixel[0] *= alphamult;
							pixel[1] *= alphamult;
							pixel[2] *= alphamult;
							pixel[3] *= alphamult;
						}
					}
				}
				break;

			case CHROMAKEY_YCBCR:
				for (int y = 0; y < height; ++y)
				{
					unsigned char *pixel = image->scanLine(y);

					for (int x = 0; x < width; ++x, pixel += 4, pc += 3)
					{
						int db = (int) pc[1] - mask.u[1];
						int dr = (int) pc[2] - mask.u[2];
						float tmp = sqrt(db * db + dr * dr);

						if (tmp <= threshold)
						{
							pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
						}
						else if (tmp <= threshold + halothreshold)
						{
							float alphamult = (tmp - threshold) / halothreshold;

							pixel[0] *= alphamult;
							pixel[1] *= alphamult;
							pixel[2] *= alphamult;
							pixel[3] *= alphamult;
						}
					}
				}
				break;

			case CHROMAKEY_CIE_LCH_L:
				for (int y = 0; y < height; ++y)
				{
					unsigned char *pixel = image->scanLine(y);

					for (int x = 0; x < width; ++x, pixel += 4, pf += 3)
					{
						float tmp = fabs(pf[0] - mask.f[0]);

						if (tmp <= threshold)
						{
							pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
						}
						else if (tmp <= threshold + halothreshold)
						{
							float alphamult = (tmp - threshold) / halothreshold;

							pixel[0] *= alphamult;
							pixel[1] *= alphamult;
							pixel[2] *= alphamult;
							pixel[3] *= alphamult;
						}
					}
				}
				break;

			case CHROMAKEY_CIE_LCH_C:
				for (int y = 0; y < height; ++y)
				{
					unsigned char *pixel = image->scanLine(y);

					for (int x = 0; x < width; ++x, pixel += 4, pf += 3)
					{
						float tmp = fabs(pf[1] - mask.f[1]);

						if (tmp <= threshold)
						{
							pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
						}
						else if (tmp <= threshold + halothreshold)
						{
							float alphamult = (tmp - threshold) / halothreshold;

							pixel[0] *= alphamult;
							pixel[1] *= alphamult;
							pixel[2] *= alphamult;
							pixel[3] *= alphamult;
						}
					}
				}
				break;

			case CHROMAKEY_CIE_LCH_H:
				for (int y = 0; y < height; ++y)
				{
					unsigned char *pixel = image->scanLine(y);

					for (int x = 0; x < width; ++x, pixel += 4, pf += 3)
					{
						// Hues in LCH(ab) are an angle on a color wheel.
						// We are tring to find the angular distance
						// between the two angles. It can never be more
						// than 180 degrees - if it is, there is a closer
						// angle that can be calculated by going in the
						// other diretion, which  can be found by
						// subtracting the angle we have from 360.
						float tmp = fabs(pf[2] - mask.f[2]);

						if (tmp > 180.0)
							tmp = 360.0 - tmp;
						if (tmp <= threshold)
							pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
					}
				}
				break;

			case CHROMAKEY_CIE_DISTANCE:
				{
					float KL = 1.0;
					float KC = 1.0;
					float KH = 1.0;
					float pi = 4 * std::atan(1);

					float L1 = ((float) mask.u[0]) / 2.55;
					float a1 = mask.u[1] - 127;
					float b1 = mask.u[2] - 127;
					float C1 = std::sqrt(a1 * a1 + b1 * b1);

					for (int y = 0; y < height; ++y)
					{
						unsigned char *pixel = image->scanLine(y);

						for (int x = 0; x < width; ++x, pixel += 4, pc += 3)
						{
							float L2 = ((float) pc[0]) / 2.55;
							int   a2 = pc[1] - 127;
							int   b2 = pc[2] - 127;
							float C2 = std::sqrt(a2 * a2 + b2 * b2);

							float delta_L_prime = L2 - L1;
							float L_bar = (L1 + L2) / 2;
							float C_bar = (C1 + C2) / 2;

							float a_prime_multiplier = 1 + 0.5 * (1 - std::sqrt(C_bar / (C_bar + 25)));
							float a1_prime = a1 * a_prime_multiplier;
							float a2_prime = a2 * a_prime_multiplier;

							float C1_prime = std::sqrt(a1_prime * a1_prime + b1 * b1);
							float C2_prime = std::sqrt(a2_prime * a2_prime + b2 * b2);
							float C_prime_bar = (C1_prime + C2_prime) / 2;
							float delta_C_prime = C2_prime - C1_prime;

							float h1_prime = std::atan2(b1, a1_prime) * 180 / pi;
							float h2_prime = std::atan2(b2, a2_prime) * 180 / pi;

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

							float delta_H_prime = 2 * std::sqrt(C1_prime * C2_prime) * std::sin(delta_h_prime * pi / 360);

							float T = 1
								- 0.17 * std::cos((H_prime_bar - 30) * pi / 180)
								+ 0.24 * std::cos(H_prime_bar * pi / 90)
								+ 0.32 * std::cos((3 * H_prime_bar + 6) * pi / 180)
								- 0.20 * std::cos((4 * H_prime_bar - 64) * pi / 180);

							float SL = 1 + 0.015 * std::pow(L_bar - 50, 2) / std::sqrt(20 + std::pow(L_bar - 50, 2));
							float SC = 1 + 0.045 * C_prime_bar;
							float SH = 1 + 0.015 * C_prime_bar * T;
							float RT = -2 * std::sqrt(C_prime_bar / (C_prime_bar + 25)) * std::sin(pi / 3 * std::exp(-std::pow((H_prime_bar - 275) / 25, 2)));
							float delta_E = std::sqrt(std::pow(delta_L_prime / KL / SL, 2)
										+ std::pow(delta_C_prime / KC / SC, 2)
										+ std::pow(delta_h_prime / KH / SH, 2)
										+ RT * delta_C_prime / KC / SC * delta_H_prime / KH / SH);
							if (delta_E <= threshold)
							{
								pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
							}
							else if (delta_E <= threshold + halothreshold)
							{
								float alphamult = (delta_E - threshold) / halothreshold;

								pixel[0] *= alphamult;
								pixel[1] *= alphamult;
								pixel[2] *= alphamult;
								pixel[3] *= alphamult;
							}
						}
					}
				}
				break;
			}

			return frame;
		}
	}
#endif

	// Loop through pixels
	for (int y = 0; y < height; ++y)
	{
		unsigned char * pixel = image->scanLine(y);

		for (int x = 0; x < width; ++x, pixel += 4)
		{
			float A = pixel[3];
			unsigned char R = (pixel[0] / A) * 255.0;
			unsigned char G = (pixel[1] / A) * 255.0;
			unsigned char B = (pixel[2] / A) * 255.0;

			// Get distance between mask color and pixel color
			long distance = Color::GetDistance((long)R, (long)G, (long)B, mask_R, mask_G, mask_B);

			if (distance <= threshold) {
				// MATCHED - Make pixel transparent
				// Due to premultiplied alpha, we must also zero out
				// the individual color channels (or else artifacts are left behind)
				pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
			}
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
	root["halo"] = halo.JsonValue();
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
	if (!root["halo"].isNull())
		halo.SetJsonValue(root["halo"]);
	if (!root["keymethod"].isNull())
		method = (ChromaKeyMethod) root["keymethod"].asInt();
}

// Get all properties for a specific frame
std::string ChromaKey::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["color"] = add_property_json("Key Color", 0.0, "color", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["fuzz"] = add_property_json("Threshold", fuzz.GetValue(requested_frame), "float", "", &fuzz, 0, 125, false, requested_frame);
	root["halo"] = add_property_json("Halo", halo.GetValue(requested_frame), "float", "", &halo, 0, 125, false, requested_frame);
	root["keymethod"] = add_property_json("Key Method", method, "int", "", NULL, 0, CHROMAKEY_LAST_METHOD, false, requested_frame);
	root["keymethod"]["choices"].append(add_property_choice_json("Basic keying", 0, method));
	root["keymethod"]["choices"].append(add_property_choice_json("HSV/HSL hue", 1, method));
	root["keymethod"]["choices"].append(add_property_choice_json("HSV saturation", 2, method));
	root["keymethod"]["choices"].append(add_property_choice_json("HSL saturation", 3, method));
	root["keymethod"]["choices"].append(add_property_choice_json("HSV value", 4, method));
	root["keymethod"]["choices"].append(add_property_choice_json("HSL luminance", 5, method));
	root["keymethod"]["choices"].append(add_property_choice_json("LCH luminosity", 6, method));
	root["keymethod"]["choices"].append(add_property_choice_json("LCH chroma", 7, method));
	root["keymethod"]["choices"].append(add_property_choice_json("LCH hue", 8, method));
	root["keymethod"]["choices"].append(add_property_choice_json("CIE Distance", 9, method));
	root["keymethod"]["choices"].append(add_property_choice_json("Cb,Cr vector", 10, method));

	// Return formatted string
	return root.toStyledString();
}
