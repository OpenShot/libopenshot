/**
 * @file
 * @brief Source file for Caption effect class
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

#include "../../include/effects/Caption.h"
#include "../../include/Clip.h"
#include "../../include/Timeline.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Caption::Caption() : color("#ffffff"), stroke("#a9a9a9"), left(0.25), top(0.8), right(0.1), bottom(0.1), stroke_width(0.001), font_size(30.0), is_dirty(true) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Caption::Caption(Color color, std::string captions, std::string format) :
		color(color), caption_text(captions), caption_format(format), stroke("#a9a9a9"), left(0.25), top(0.8), right(0.1), bottom(0.1), stroke_width(0.001), font_size(30.0), is_dirty(true)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Caption::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Caption";
	info.name = "Caption";
	info.description = "Add text captions on top of your video.";
	info.has_audio = false;
	info.has_video = true;
}

// Set the caption string to use (see VTT format)
std::string Caption::CaptionText() {
	return caption_text;
}

// Get the caption string
void Caption::CaptionText(std::string new_caption_text) {
	caption_text = new_caption_text;
	is_dirty = true;
}

// Set the caption format to use (only VTT format is currently supported)
std::string Caption::CaptionFormat() {
	return caption_format;
}

// Get the caption format
void Caption::CaptionFormat(std::string new_caption_format) {
	caption_format = new_caption_format;
	is_dirty = true;
}

// Process regex string only when dirty
void Caption::process_regex() {
	if (is_dirty) {
		is_dirty = false;

		// Clear existing matches
		matchedCaptions.clear();

		// Parse regex and find all matches
		QRegularExpression allPathsRegex(QStringLiteral("(\\d{2})?:*(\\d{2}):(\\d{2}).(\\d{2,3})\\s*-->\\s*(\\d{2})?:*(\\d{2}):(\\d{2}).(\\d{2,3})([\\s\\S]*?)\\n(.*?)(?=\\n\\d{2,3}|\\Z)"), QRegularExpression::MultilineOption);
		QRegularExpressionMatchIterator i = allPathsRegex.globalMatch(QString(caption_text.c_str()));
		while (i.hasNext()) {
			QRegularExpressionMatch match = i.next();
			if (match.hasMatch()) {
				// Push all match objects into a vector (so we can reverse them later)
				matchedCaptions.push_back(match);
			}
		}
	}
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Caption::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Process regex (if needed)
	process_regex();

	// Get the Clip and Timeline pointers (if available)
	Clip* clip = (Clip*) ParentClip();
	Timeline* timeline = NULL;
	Fraction fps;
	double scale_factor = 1.0; // amount of scaling needed for text (based on preview window size)
	if (clip->ParentTimeline() != NULL) {
		timeline = (Timeline*) clip->ParentTimeline();
	} else if (this->ParentTimeline() != NULL) {
		timeline = (Timeline*) this->ParentTimeline();
	}

	// Get the FPS from the parent object (Timeline or Clip's Reader)
	if (timeline != NULL) {
		fps.num = timeline->info.fps.num;
		fps.den = timeline->info.fps.den;
		scale_factor = (double) timeline->preview_width / (double) timeline->info.width;
	} else if (clip != NULL && clip->Reader() != NULL) {
		fps.num = clip->Reader()->info.fps.num;
		fps.den = clip->Reader()->info.fps.den;
		scale_factor = 1.0;
	}

	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Load timeline's new frame image into a QPainter
	QPainter painter(frame_image.get());
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing, true);

	// Composite a new layer onto the image
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

	// Stroke / border pen
	if (stroke_width.GetValue(frame_number) > 0.0) {
		QPen pen;
		pen.setColor(QColor(QString(stroke.GetColorHex(frame_number).c_str())));
		pen.setWidth(stroke_width.GetValue(frame_number) * scale_factor);
		painter.setPen(pen);
	}

	// Fill color brush
	QBrush brush;
	brush.setColor(QColor(QString(color.GetColorHex(frame_number).c_str())));
	brush.setStyle(Qt::SolidPattern);
	painter.setBrush(brush);

	// Font options for caption
	// TODO: Allow more font options (family, bold, style)
	QFont font;
	if (font_size.GetValue(frame_number) > 0.0) {
		font.setPointSizeF(font_size.GetValue(frame_number) * scale_factor);
	} else {
		// Font can't be 0 sized
		font.setPointSizeF(1.0);
	}

	// Loop through matches and find text to display (if any)
	for (auto match = matchedCaptions.begin(); match != matchedCaptions.end(); match++) {

		// Build timestamp (00:00:04.000 --> 00:00:06.500)
		int64_t start_frame = ((match->captured(1).toFloat() * 60.0 * 60.0 ) + (match->captured(2).toFloat() * 60.0 ) +
							   match->captured(3).toFloat() + (match->captured(4).toFloat() / 1000.0)) * fps.ToFloat();
		int64_t end_frame = ((match->captured(5).toFloat() * 60.0 * 60.0 ) + (match->captured(6).toFloat() * 60.0 ) +
							 match->captured(7).toFloat() + (match->captured(8).toFloat() / 1000.0)) * fps.ToFloat();

		// Get current keyframe values
		double left_value = left.GetValue(frame_number);
		double top_value = top.GetValue(frame_number);

		// TODO: Use all 4 margins and wrap text
		double right_value = right.GetValue(frame_number);
		double bottom_value = bottom.GetValue(frame_number);

		// Parse WEBVTT caption format
		double starting_x = frame_image->width() * left_value;
		double starting_y = frame_image->height() * top_value;;

		// Split multiple lines into separate paths
		QStringList lines = match->captured(9).split("\n");
		for(int index = 0; index < lines.length(); index++) {
			// Multi-line
			QString line = lines[index];
			// Ignore lines that start with NOTE, or are <= 1 char long
			if (!line.startsWith(QStringLiteral("NOTE")) &&
				!line.isEmpty() && frame_number >= start_frame && frame_number <= end_frame &&
				!line.length() <= 1	) {

				// Location for text
				QPoint p(starting_x, starting_y);

				// Draw text onto path (for correct border and fill)
				QPainterPath path1;
				path1.addText(p, font, line);
				painter.drawPath(path1);

				// Increment QPoint to height of text (for next line) + padding
				starting_y += path1.boundingRect().height() + (10.0 * scale_factor);
			}
		}
	}

	// End painter
	painter.end();

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Caption::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Caption::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["color"] = color.JsonValue();
	root["stroke"] = stroke.JsonValue();
	root["stroke_width"] = stroke_width.JsonValue();
	root["font_size"] = font_size.JsonValue();
	root["left"] = left.JsonValue();
	root["top"] = top.JsonValue();
	root["right"] = right.JsonValue();
	root["bottom"] = bottom.JsonValue();
	root["caption_text"] = caption_text;
	root["caption_format"] = caption_format;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Caption::SetJson(const std::string value) {

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
void Caption::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
	if (!root["stroke"].isNull())
		stroke.SetJsonValue(root["stroke"]);
	if (!root["stroke_width"].isNull())
		stroke_width.SetJsonValue(root["stroke_width"]);
	if (!root["font_size"].isNull())
		font_size.SetJsonValue(root["font_size"]);
	if (!root["left"].isNull())
		left.SetJsonValue(root["left"]);
	if (!root["top"].isNull())
		top.SetJsonValue(root["top"]);
	if (!root["right"].isNull())
		right.SetJsonValue(root["right"]);
	if (!root["bottom"].isNull())
		bottom.SetJsonValue(root["bottom"]);
	if (!root["caption_text"].isNull())
		caption_text = root["caption_text"].asString();
	if (!root["caption_format"].isNull())
		caption_format = root["caption_format"].asString();

	// Mark effect as dirty to reparse Regex
	is_dirty = true;
}

// Get all properties for a specific frame
std::string Caption::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["color"] = add_property_json("Color", 0.0, "color", "", NULL, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["stroke"] = add_property_json("Border", 0.0, "color", "", NULL, 0, 255, false, requested_frame);
	root["stroke"]["red"] = add_property_json("Red", stroke.red.GetValue(requested_frame), "float", "", &stroke.red, 0, 255, false, requested_frame);
	root["stroke"]["blue"] = add_property_json("Blue", stroke.blue.GetValue(requested_frame), "float", "", &stroke.blue, 0, 255, false, requested_frame);
	root["stroke"]["green"] = add_property_json("Green", stroke.green.GetValue(requested_frame), "float", "", &stroke.green, 0, 255, false, requested_frame);
	root["stroke_width"] = add_property_json("Stroke Width", stroke_width.GetValue(requested_frame), "float", "", &stroke_width, 0, 10.0, false, requested_frame);
	root["font_size"] = add_property_json("Font Size", font_size.GetValue(requested_frame), "float", "", &font_size, 0, 200.0, false, requested_frame);
	root["left"] = add_property_json("Left Size", left.GetValue(requested_frame), "float", "", &left, 0.0, 0.5, false, requested_frame);
	root["top"] = add_property_json("Top Size", top.GetValue(requested_frame), "float", "", &top, 0.0, 0.5, false, requested_frame);
	root["right"] = add_property_json("Right Size", right.GetValue(requested_frame), "float", "", &right, 0.0, 0.5, false, requested_frame);
	root["bottom"] = add_property_json("Bottom Size", bottom.GetValue(requested_frame), "float", "", &bottom, 0.0, 0.5, false, requested_frame);
	root["caption_text"] = add_property_json("Captions", 0.0, "string", caption_text, NULL, -1, -1, false, requested_frame);
	root["caption_format"] = add_property_json("Format", 0.0, "string", caption_format, NULL, -1, -1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
