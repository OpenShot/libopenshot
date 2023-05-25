/**
 * @file
 * @brief Source file for Crop effect class (cropping any side, with x/y offsets)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Crop.h"
#include "Exceptions.h"
#include "KeyFrame.h"

#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QRect>
#include <QSize>

using namespace openshot;

/// Default constructor, useful when using Json to load the effect properties
Crop::Crop() : Crop::Crop(0.0, 0.0, 0.0, 0.0, 0.0, 0.0) {}

Crop::Crop(
	Keyframe left, Keyframe top,
	Keyframe right, Keyframe bottom,
	Keyframe x, Keyframe y) :
		left(left), top(top), right(right), bottom(bottom), x(x), y(y), resize(false)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Crop::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Crop";
	info.name = "Crop";
	info.description = "Crop out any part of your video.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Crop::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Get current keyframe values
	double left_value = left.GetValue(frame_number);
	double top_value = top.GetValue(frame_number);
	double right_value = right.GetValue(frame_number);
	double bottom_value = bottom.GetValue(frame_number);

	// Get the current shift amount
	double x_shift = x.GetValue(frame_number);
	double y_shift = y.GetValue(frame_number);

	QSize sz = frame_image->size();

	// Compute destination rectangle to paint into
	QRectF paint_r(
			left_value * sz.width(), top_value * sz.height(),
			std::max(0.0, 1.0 - left_value - right_value) * sz.width(),
			std::max(0.0, 1.0 - top_value - bottom_value) * sz.height());

	// Copy rectangle is destination translated by offsets
	QRectF copy_r = paint_r;
	copy_r.translate(x_shift * sz.width(), y_shift * sz.height());

	// Constrain offset copy rect to stay within image borders
	if (copy_r.left() < 0) {
		paint_r.setLeft(paint_r.left() - copy_r.left());
		copy_r.setLeft(0);
	}
	if (copy_r.right() > sz.width()) {
		paint_r.setRight(paint_r.right() - (copy_r.right() - sz.width()));
		copy_r.setRight(sz.width());
	}
	if (copy_r.top() < 0) {
		paint_r.setTop(paint_r.top() - copy_r.top());
		copy_r.setTop(0);
	}
	if (copy_r.bottom() > sz.height()) {
		paint_r.setBottom(paint_r.bottom() - (copy_r.bottom() - sz.height()));
		copy_r.setBottom(sz.height());
	}

	QImage cropped(sz, QImage::Format_RGBA8888_Premultiplied);
	cropped.fill(Qt::transparent);

	QPainter p(&cropped);
	p.drawImage(paint_r, *frame_image, copy_r);
	p.end();

	if (resize) {
		// Resize image to match cropped QRect (reduce frame size)
		frame->AddImage(std::make_shared<QImage>(cropped.copy(paint_r.toRect())));
	} else {
		// Copy cropped image into transparent frame image (maintain frame size)
		frame->AddImage(std::make_shared<QImage>(cropped.copy()));
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Crop::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Crop::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["left"] = left.JsonValue();
	root["top"] = top.JsonValue();
	root["right"] = right.JsonValue();
	root["bottom"] = bottom.JsonValue();
	root["x"] = x.JsonValue();
	root["y"] = y.JsonValue();
	root["resize"] = resize;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Crop::SetJson(const std::string value) {

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
void Crop::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["left"].isNull())
		left.SetJsonValue(root["left"]);
	if (!root["top"].isNull())
		top.SetJsonValue(root["top"]);
	if (!root["right"].isNull())
		right.SetJsonValue(root["right"]);
	if (!root["bottom"].isNull())
		bottom.SetJsonValue(root["bottom"]);
	if (!root["x"].isNull())
		x.SetJsonValue(root["x"]);
	if (!root["y"].isNull())
		y.SetJsonValue(root["y"]);
	if (!root["resize"].isNull())
		resize = root["resize"].asBool();
}

// Get all properties for a specific frame
std::string Crop::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["left"] = add_property_json("Left Size", left.GetValue(requested_frame), "float", "", &left, 0.0, 1.0, false, requested_frame);
	root["top"] = add_property_json("Top Size", top.GetValue(requested_frame), "float", "", &top, 0.0, 1.0, false, requested_frame);
	root["right"] = add_property_json("Right Size", right.GetValue(requested_frame), "float", "", &right, 0.0, 1.0, false, requested_frame);
	root["bottom"] = add_property_json("Bottom Size", bottom.GetValue(requested_frame), "float", "", &bottom, 0.0, 1.0, false, requested_frame);
	root["x"] = add_property_json("X Offset", x.GetValue(requested_frame), "float", "", &x, -1.0, 1.0, false, requested_frame);
	root["y"] = add_property_json("Y Offset", y.GetValue(requested_frame), "float", "", &y, -1.0, 1.0, false, requested_frame);

	// Add replace_image choices (dropdown style)
	root["resize"] = add_property_json("Resize Image", resize, "int", "", NULL, 0, 1, false, requested_frame);
	root["resize"]["choices"].append(add_property_choice_json("Yes", true, resize));
	root["resize"]["choices"].append(add_property_choice_json("No", false, resize));

	// Return formatted string
	return root.toStyledString();
}
