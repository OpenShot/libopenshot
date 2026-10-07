/**
 * @file
 * @brief Source file for the TrackedObjectBBox class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <fstream>

#include "TrackedObjectBBox.h"

#include "Clip.h"
#include "TimelineBase.h"

#include "trackerdata.pb.h"
#include <google/protobuf/util/time_util.h>

using google::protobuf::util::TimeUtil;

using namespace openshot;

namespace {
std::vector<uint32_t> encode_object_mask_rle(const std::vector<uint8_t>& mask)
{
	std::vector<uint32_t> rle;
	uint8_t current = 0;
	uint32_t count = 0;
	for (uint8_t value : mask) {
		value = value ? 1 : 0;
		if (value == current) {
			++count;
		} else {
			rle.push_back(count);
			current = value;
			count = 1;
		}
	}
	rle.push_back(count);
	return rle;
}

std::vector<uint8_t> decode_object_mask_rle(const ObjectMaskData& mask)
{
	std::vector<uint8_t> decoded(static_cast<size_t>(mask.width * mask.height), 0);
	int offset = 0;
	bool value = false;
	for (uint32_t count : mask.rle) {
		const int end = std::min(mask.width * mask.height, offset + static_cast<int>(count));
		if (value)
			std::fill(decoded.begin() + offset, decoded.begin() + end, static_cast<uint8_t>(1));
		offset = end;
		value = !value;
		if (offset >= mask.width * mask.height)
			break;
	}
	return decoded;
}

ObjectMaskData transform_mask_between_boxes(
	const ObjectMaskData& source_mask,
	const BBox& source_box,
	const BBox& target_box)
{
	ObjectMaskData result;
	if (!source_mask.HasData() ||
		source_box.width <= 0.0f || source_box.height <= 0.0f ||
		target_box.width <= 0.0f || target_box.height <= 0.0f)
		return result;

	const float source_left = (source_box.cx - source_box.width / 2.0f) * source_mask.width;
	const float source_top = (source_box.cy - source_box.height / 2.0f) * source_mask.height;
	const float source_width = source_box.width * source_mask.width;
	const float source_height = source_box.height * source_mask.height;
	const float target_left = (target_box.cx - target_box.width / 2.0f) * source_mask.width;
	const float target_top = (target_box.cy - target_box.height / 2.0f) * source_mask.height;
	const float target_width = target_box.width * source_mask.width;
	const float target_height = target_box.height * source_mask.height;
	if (source_width <= 0.0f || source_height <= 0.0f || target_width <= 0.0f || target_height <= 0.0f)
		return result;

	const std::vector<uint8_t> source = decode_object_mask_rle(source_mask);
	std::vector<uint8_t> transformed(static_cast<size_t>(source_mask.width * source_mask.height), 0);
	const int min_x = std::max(0, static_cast<int>(std::floor(target_left)));
	const int min_y = std::max(0, static_cast<int>(std::floor(target_top)));
	const int max_x = std::min(source_mask.width, static_cast<int>(std::ceil(target_left + target_width)));
	const int max_y = std::min(source_mask.height, static_cast<int>(std::ceil(target_top + target_height)));
	for (int y = min_y; y < max_y; ++y) {
		for (int x = min_x; x < max_x; ++x) {
			const float source_x = source_left + (static_cast<float>(x) - target_left) * source_width / target_width;
			const float source_y = source_top + (static_cast<float>(y) - target_top) * source_height / target_height;
			const int sx = static_cast<int>(std::round(source_x));
			const int sy = static_cast<int>(std::round(source_y));
			if (sx < 0 || sx >= source_mask.width || sy < 0 || sy >= source_mask.height)
				continue;
			if (source[static_cast<size_t>(sy * source_mask.width + sx)])
				transformed[static_cast<size_t>(y * source_mask.width + x)] = 1;
		}
	}

	if (std::none_of(transformed.begin(), transformed.end(), [](uint8_t value) { return value != 0; }))
		return result;
	result.width = source_mask.width;
	result.height = source_mask.height;
	result.rle = encode_object_mask_rle(transformed);
	return result;
}
}

// Default Constructor, delegating
TrackedObjectBBox::TrackedObjectBBox()
	: TrackedObjectBBox::TrackedObjectBBox(0, 0, 255, 255) {}

// Constructor that takes RGBA values for stroke, and sets the bounding-box
// displacement as 0 and the scales as 1 for the first frame
TrackedObjectBBox::TrackedObjectBBox(int Red, int Green, int Blue, int Alfa)
	: delta_x(0.0), delta_y(0.0),
	  scale_x(1.0), scale_y(1.0),
	  background_alpha(0.0), background_corner(12),
	  mask_alpha(120.0 / 255.0),
	  stroke_width(2) , stroke_alpha(0.7),
	  stroke(Red, Green, Blue, Alfa),
	  background(Red, Green, Blue, Alfa),
	  mask_color(Red, Green, Blue, Alfa)
{
	this->TimeScale = 1.0;
}

// Add a BBox to the BoxVec map
void TrackedObjectBBox::AddBox(int64_t _frame_num, float _cx, float _cy, float _width, float _height, float _angle)
{
	// Check if the given frame number is valid
	if (_frame_num < 0)
		return;

	// Instantiate a new bounding-box
	BBox newBBox = BBox(_cx, _cy, _width, _height, _angle);

	// Get the time of given frame
	double time = this->FrameNToTime(_frame_num, 1.0);
	// Create an iterator that points to the BoxVec pair indexed by the time of given frame
	auto BBoxIterator = BoxVec.find(time);

	if (BBoxIterator != BoxVec.end())
	{
		// There is a bounding-box indexed by the time of given frame, update-it
		BBoxIterator->second = newBBox;
	}
	else
	{
		// There isn't a bounding-box indexed by the time of given frame, insert a new one
		BoxVec.insert({time, newBBox});
	}
}

void TrackedObjectBBox::AddMask(int64_t frame_num, const ObjectMaskData& mask)
{
	if (frame_num < 0 || !mask.HasData())
		return;

	double time = FrameNToTime(frame_num, 1.0);
	MaskVec[time] = mask;
}

bool TrackedObjectBBox::HasMask(int64_t frame_num, int64_t max_frame_gap) const
{
	return GetMask(frame_num, max_frame_gap).HasData();
}

bool TrackedObjectBBox::HasMaskData() const
{
	return !MaskVec.empty();
}

ObjectMaskData TrackedObjectBBox::GetMask(int64_t frame_num, int64_t max_frame_gap) const
{
	double time = FrameNToTime(frame_num, 1.0);
	auto it = MaskVec.find(time);
	if (it != MaskVec.end())
		return it->second;
	if (max_frame_gap <= 0 || MaskVec.empty())
		return {};

	auto after = MaskVec.lower_bound(time);
	if (after == MaskVec.begin())
		return {};

	auto before = std::prev(after);
	double max_gap_time = FrameNToTime(max_frame_gap, 1.0) - FrameNToTime(0, 1.0);
	if (time - before->first <= max_gap_time + 0.000001) {
		auto source_box = BoxVec.find(before->first);
		auto target_box = BoxVec.find(time);
		if (source_box != BoxVec.end() && target_box != BoxVec.end())
			return transform_mask_between_boxes(before->second, source_box->second, target_box->second);
		return before->second;
	}
	return {};
}

// Get the size of BoxVec map
int64_t TrackedObjectBBox::GetLength() const
{
	if (BoxVec.empty())
		return 0;
	if (BoxVec.size() == 1)
		return 1;
	return BoxVec.size();
}

// Check if there is a bounding-box in the given frame
bool TrackedObjectBBox::Contains(int64_t frame_num) const
{
	// Get the time of given frame
	double time = this->FrameNToTime(frame_num, 1.0);
	// Create an iterator that points to the BoxVec pair indexed by the time of given frame (or the closest time)
	auto it = BoxVec.lower_bound(time);
	if (it == BoxVec.end()){
		// BoxVec pair not found
		return false;
	}
	return true;
}

// Check if there is a bounding-box in the exact frame number
bool TrackedObjectBBox::ExactlyContains(int64_t frame_number) const
{
	// Get the time of given frame
	double time = FrameNToTime(frame_number, 1.0);
	// Create an iterator that points to the BoxVec pair indexed by the exact time of given frame
	auto it = BoxVec.find(time);
	if (it == BoxVec.end()){
		// BoxVec pair not found
		return false;
	}
	return true;
}

// Remove a bounding-box from the BoxVec map
void TrackedObjectBBox::RemoveBox(int64_t frame_number)
{
	// Get the time of given frame
	double time = this->FrameNToTime(frame_number, 1.0);
	// Create an iterator that points to the BoxVec pair indexed by the time of given frame
	auto it = BoxVec.find(time);
	if (it != BoxVec.end())
	{
		// The BoxVec pair exists, so remove it
		BoxVec.erase(time);
	}
	return;
}

// Return a bounding-box from BoxVec with it's properties adjusted by the Keyframes
BBox TrackedObjectBBox::GetBox(int64_t frame_number)
{
	// Get the time position of the given frame.
	double time = this->FrameNToTime(frame_number, this->TimeScale);

	// Return a iterator pointing to the BoxVec pair indexed by time or to the pair indexed
	// by the closest upper time value.
	auto currentBBoxIterator = BoxVec.lower_bound(time);

	// Check if there is a pair indexed by time, returns an empty bbox if there isn't.
	if (currentBBoxIterator == BoxVec.end())
	{
		// Create and return an empty bounding-box object
		BBox emptyBBox;
		return emptyBBox;
	}

	// Check if the iterator matches a BBox indexed by time or points to the first element of BoxVec
	if ((currentBBoxIterator->first == time) || (currentBBoxIterator == BoxVec.begin()))
	{
		// Get the BBox indexed by time
		BBox currentBBox = currentBBoxIterator->second;

		// Adjust the BBox properties by the Keyframes values
		currentBBox.cx += this->delta_x.GetValue(frame_number);
		currentBBox.cy += this->delta_y.GetValue(frame_number);
		currentBBox.width *= this->scale_x.GetValue(frame_number);
		currentBBox.height *= this->scale_y.GetValue(frame_number);
		return currentBBox;
	}

	// BBox indexed by the closest upper time
	BBox currentBBox = currentBBoxIterator->second;
	// BBox indexed by the closet lower time
	BBox previousBBox = prev(currentBBoxIterator, 1)->second;

	// Interpolate a BBox in the middle of previousBBox and currentBBox
	BBox interpolatedBBox = InterpolateBoxes(prev(currentBBoxIterator, 1)->first, currentBBoxIterator->first,
											 previousBBox, currentBBox, time);

	// Adjust the BBox properties by the Keyframes values
	interpolatedBBox.cx += this->delta_x.GetValue(frame_number);
	interpolatedBBox.cy += this->delta_y.GetValue(frame_number);
	interpolatedBBox.width *= this->scale_x.GetValue(frame_number);
	interpolatedBBox.height *= this->scale_y.GetValue(frame_number);
	return interpolatedBBox;
}

double TrackedObjectBBox::ScaledStrokeWidth(int64_t frame_number, int image_width, int image_height) const
{
	const double base_width = std::max(0.0, static_cast<double>(stroke_width.GetValue(frame_number)));
	Clip* parent_clip = static_cast<Clip*>(ParentClip());
	if (!parent_clip || image_width <= 0 || image_height <= 0)
		return base_width;

	int target_width = image_width;
	int target_height = image_height;
	if (parent_clip->ParentTimeline()) {
		TimelineBase* timeline = static_cast<TimelineBase*>(parent_clip->ParentTimeline());
		if (timeline->preview_width > 0)
			target_width = timeline->preview_width;
		if (timeline->preview_height > 0)
			target_height = timeline->preview_height;
	}

	QSize output_size(image_width, image_height);
	switch (parent_clip->scale) {
		case SCALE_FIT:
			output_size.scale(target_width, target_height, Qt::KeepAspectRatio);
			break;
		case SCALE_STRETCH:
			output_size.scale(target_width, target_height, Qt::IgnoreAspectRatio);
			break;
		case SCALE_CROP:
			output_size.scale(target_width, target_height, Qt::KeepAspectRatioByExpanding);
			break;
		case SCALE_NONE:
		default:
			break;
	}
	if (output_size.width() <= 0 || output_size.height() <= 0)
		return base_width;

	const double raster_scale_x = static_cast<double>(output_size.width()) / image_width;
	const double raster_scale_y = static_cast<double>(output_size.height()) / image_height;
	const double raster_scale = std::sqrt(raster_scale_x * raster_scale_y);
	return base_width * std::max(raster_scale, 1.0 / std::max(raster_scale, 0.000001));
}

// Interpolate the bouding-boxes properties
BBox TrackedObjectBBox::InterpolateBoxes(double t1, double t2, BBox left, BBox right, double target)
{
	// Interpolate the x-coordinate of the center point
	Point cx_left(t1, left.cx, openshot::InterpolationType::LINEAR);
	Point cx_right(t2, right.cx, openshot::InterpolationType::LINEAR);
	Point cx = InterpolateBetween(cx_left, cx_right, target, 0.01);

	// Interpolate de y-coordinate of the center point
	Point cy_left(t1, left.cy, openshot::InterpolationType::LINEAR);
	Point cy_right(t2, right.cy, openshot::InterpolationType::LINEAR);
	Point cy = InterpolateBetween(cy_left, cy_right, target, 0.01);

	// Interpolate the width
	Point width_left(t1, left.width, openshot::InterpolationType::LINEAR);
	Point width_right(t2, right.width, openshot::InterpolationType::LINEAR);
	Point width = InterpolateBetween(width_left, width_right, target, 0.01);

	// Interpolate the height
	Point height_left(t1, left.height, openshot::InterpolationType::LINEAR);
	Point height_right(t2, right.height, openshot::InterpolationType::LINEAR);
	Point height = InterpolateBetween(height_left, height_right, target, 0.01);

	// Interpolate the source bounding-box angle
	Point angle_left(t1, left.angle, openshot::InterpolationType::LINEAR);
	Point angle_right(t1, right.angle, openshot::InterpolationType::LINEAR);
	Point angle = InterpolateBetween(angle_left, angle_right, target, 0.01);

	// Create a bounding box with the interpolated points
	BBox interpolatedBox(cx.co.Y, cy.co.Y, width.co.Y, height.co.Y, angle.co.Y);

	return interpolatedBox;
}

// Update object's BaseFps
void TrackedObjectBBox::SetBaseFPS(Fraction fps){
	this->BaseFps = fps;
	return;
}

// Return the object's BaseFps
Fraction TrackedObjectBBox::GetBaseFPS(){
	return BaseFps;
}

// Get the time of the given frame
double TrackedObjectBBox::FrameNToTime(int64_t frame_number, double time_scale) const{
	double time = ((double)frame_number) * this->BaseFps.Reciprocal().ToDouble() * (1.0 / time_scale);

	return time;
}

// Update the TimeScale member variable
void TrackedObjectBBox::ScalePoints(double time_scale){
	this->TimeScale = time_scale;
}

// Load the bounding-boxes information from the protobuf file
bool TrackedObjectBBox::LoadBoxData(std::string inputFilePath)
{
	using std::ios;

	// Variable to hold the loaded data
	pb_tracker::Tracker bboxMessage;

	// Read the existing tracker message.
	std::fstream input(inputFilePath, ios::in | ios::binary);

	// Check if it was able to read the protobuf data
	if (!bboxMessage.ParseFromIstream(&input))
	{
		std::cerr << "Failed to parse protobuf message." << std::endl;
		return false;
	}

	this->clear();

	// Iterate over all frames of the saved message
	for (size_t i = 0; i < bboxMessage.frame_size(); i++)
	{
		// Get data of the i-th frame
		const pb_tracker::Frame &pbFrameData = bboxMessage.frame(i);

		// Get frame number
		size_t frame_number = pbFrameData.id();

		// Get bounding box data from current frame
		const pb_tracker::Frame::Box &box = pbFrameData.bounding_box();

		float width = box.x2() - box.x1();
		float height = box.y2() - box.y1();
		float cx = box.x1() + width/2;
		float cy = box.y1() + height/2;
		float angle = 0.0;


		if ( (cx >= 0.0) && (cy >= 0.0) && (width >= 0.0) && (height >= 0.0) )
		{
			// The bounding-box properties are valid, so add it to the BoxVec map
			this->AddBox(frame_number, cx, cy, width, height, angle);
		}
	}

	// Show the time stamp from the last update in tracker data file
	if (bboxMessage.has_last_updated())
	{
		std::cout << " Loaded Data. Saved Time Stamp: "
				  << TimeUtil::ToString(bboxMessage.last_updated()) << std::endl;
	}

	// Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();

	return true;
}

// Clear the BoxVec map
void TrackedObjectBBox::clear()
{
	BoxVec.clear();
	MaskVec.clear();
}

// Generate JSON string of this object
std::string TrackedObjectBBox::Json() const
{
	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value TrackedObjectBBox::JsonValue() const
{
	// Create root json object
	Json::Value root;

	// Object's properties
	root["box_id"] = Id();
	root["BaseFPS"]["num"] = BaseFps.num;
	root["BaseFPS"]["den"] = BaseFps.den;
	root["TimeScale"] = TimeScale;

	// Keyframe's properties
	root["delta_x"] = delta_x.JsonValue();
	root["delta_y"] = delta_y.JsonValue();
	root["scale_x"] = scale_x.JsonValue();
	root["scale_y"] = scale_y.JsonValue();
	root["visible"] = visible.JsonValue();
	root["draw_box"] = draw_box.JsonValue();
	root["draw_text"] = draw_text.JsonValue();
	if (!MaskVec.empty()) {
		root["draw_mask"] = draw_mask.JsonValue();
		root["mask_alpha"] = mask_alpha.JsonValue();
		root["mask_color"] = mask_color.JsonValue();
	}
	root["stroke"] = stroke.JsonValue();
	root["background_alpha"] = background_alpha.JsonValue();
	root["background_corner"] = background_corner.JsonValue();
	root["background"] = background.JsonValue();
	root["stroke_width"] = stroke_width.JsonValue();
	root["stroke_alpha"] = stroke_alpha.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void TrackedObjectBBox::SetJson(const std::string value)
{
	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception &e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
	return;
}

// Load Json::Value into this object
void TrackedObjectBBox::SetJsonValue(const Json::Value root)
{

	// Set the Id by the given JSON object
	if (!root["box_id"].isNull() && root["box_id"].asString() != "")
		Id(root["box_id"].asString());

	// Set the BaseFps by the given JSON object
	if (!root["BaseFPS"].isNull() && root["BaseFPS"].isObject())
	{
		if (!root["BaseFPS"]["num"].isNull())
			BaseFps.num = (int)root["BaseFPS"]["num"].asInt();
		if (!root["BaseFPS"]["den"].isNull())
			BaseFps.den = (int)root["BaseFPS"]["den"].asInt();
	}
	// Set the TimeScale by the given JSON object
	if (!root["TimeScale"].isNull())
	{
		double scale = (double)root["TimeScale"].asDouble();
		this->ScalePoints(scale);
	}
	// Set the protobuf data path by the given JSON object
	if (!root["protobuf_data_path"].isNull())
		protobufDataPath = root["protobuf_data_path"].asString();

	// Set the Keyframes by the given JSON object
	if (!root["delta_x"].isNull())
		delta_x.SetJsonValue(root["delta_x"]);
	if (!root["delta_y"].isNull())
		delta_y.SetJsonValue(root["delta_y"]);
	if (!root["scale_x"].isNull())
		scale_x.SetJsonValue(root["scale_x"]);
	if (!root["scale_y"].isNull())
		scale_y.SetJsonValue(root["scale_y"]);
	if (!root["visible"].isNull())
		visible.SetJsonValue(root["visible"]);
	if (!root["draw_box"].isNull())
		draw_box.SetJsonValue(root["draw_box"]);
	if (!root["draw_text"].isNull())
		draw_text.SetJsonValue(root["draw_text"]);
	if (!root["draw_mask"].isNull())
		draw_mask.SetJsonValue(root["draw_mask"]);
	if (!root["mask_alpha"].isNull())
		mask_alpha.SetJsonValue(root["mask_alpha"]);
	if (!root["mask_color"].isNull())
		mask_color.SetJsonValue(root["mask_color"]);
	if (!root["stroke"].isNull())
		stroke.SetJsonValue(root["stroke"]);
	if (!root["background_alpha"].isNull())
		background_alpha.SetJsonValue(root["background_alpha"]);
	if (!root["background_corner"].isNull())
		background_corner.SetJsonValue(root["background_corner"]);
	if (!root["background"].isNull())
		background.SetJsonValue(root["background"]);
	if (!root["stroke_width"].isNull())
		stroke_width.SetJsonValue(root["stroke_width"]);
	if (!root["stroke_alpha"].isNull())
		stroke_alpha.SetJsonValue(root["stroke_alpha"]);
	return;
}

// Get all properties for a specific frame (perfect for a UI to display the current state
// of all properties at any time)
Json::Value TrackedObjectBBox::PropertiesJSON(int64_t requested_frame) const
{
	Json::Value root;

	BBox box = GetBox(requested_frame);

	// Add the ID of this object to the JSON object
	root["box_id"] = add_property_json("Box ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);

	// Add the data of given frame bounding-box to the JSON object
	root["x1"] = add_property_json("X1", box.cx-(box.width/2), "float", "", NULL, 0.0, 1.0, true, requested_frame);
	root["y1"] = add_property_json("Y1", box.cy-(box.height/2), "float", "", NULL, 0.0, 1.0, true, requested_frame);
	root["x2"] = add_property_json("X2", box.cx+(box.width/2), "float", "", NULL, 0.0, 1.0, true, requested_frame);
	root["y2"] = add_property_json("Y2", box.cy+(box.height/2), "float", "", NULL, 0.0, 1.0, true, requested_frame);

	// Add the bounding-box Keyframes to the JSON object
	root["delta_x"] = add_property_json("Displacement X-axis", delta_x.GetValue(requested_frame), "float", "", &delta_x, -1.0, 1.0, false, requested_frame);
	root["delta_y"] = add_property_json("Displacement Y-axis", delta_y.GetValue(requested_frame), "float", "", &delta_y, -1.0, 1.0, false, requested_frame);
	root["scale_x"] = add_property_json("Scale (Width)", scale_x.GetValue(requested_frame), "float", "", &scale_x, 0.0, 1.0, false, requested_frame);
	root["scale_y"] = add_property_json("Scale (Height)", scale_y.GetValue(requested_frame), "float", "", &scale_y, 0.0, 1.0, false, requested_frame);
	root["visible"] = add_property_json("Visible", visible.GetValue(requested_frame), "int", "", &visible, 0, 1, true, requested_frame);

	root["draw_box"] = add_property_json("Draw Box", draw_box.GetValue(requested_frame), "int", "", &draw_box, 0, 1, false, requested_frame);
	root["draw_box"]["choices"].append(add_property_choice_json("Yes", true, draw_box.GetValue(requested_frame)));
	root["draw_box"]["choices"].append(add_property_choice_json("No", false, draw_box.GetValue(requested_frame)));

	root["draw_text"] = add_property_json("Draw Text", draw_text.GetValue(requested_frame), "int", "", &draw_text, 0, 1, false, requested_frame);
	root["draw_text"]["choices"].append(add_property_choice_json("Yes", true, draw_text.GetValue(requested_frame)));
	root["draw_text"]["choices"].append(add_property_choice_json("No", false, draw_text.GetValue(requested_frame)));

	if (HasMaskData()) {
		root["draw_mask"] = add_property_json("Draw Mask", draw_mask.GetValue(requested_frame), "int", "", &draw_mask, 0, 1, false, requested_frame);
		root["draw_mask"]["choices"].append(add_property_choice_json("Yes", true, draw_mask.GetValue(requested_frame)));
		root["draw_mask"]["choices"].append(add_property_choice_json("No", false, draw_mask.GetValue(requested_frame)));

		root["mask_color"] = add_property_json("Mask Color", 0.0, "color", "", NULL, 0, 255, false, requested_frame);
		root["mask_color"]["red"] = add_property_json("Red", mask_color.red.GetValue(requested_frame), "float", "", &mask_color.red, 0, 255, false, requested_frame);
		root["mask_color"]["blue"] = add_property_json("Blue", mask_color.blue.GetValue(requested_frame), "float", "", &mask_color.blue, 0, 255, false, requested_frame);
		root["mask_color"]["green"] = add_property_json("Green", mask_color.green.GetValue(requested_frame), "float", "", &mask_color.green, 0, 255, false, requested_frame);
		root["mask_alpha"] = add_property_json("Mask Alpha", mask_alpha.GetValue(requested_frame), "float", "", &mask_alpha, 0.0, 1.0, false, requested_frame);
	}

	root["stroke"] = add_property_json("Border", 0.0, "color", "", NULL, 0, 255, false, requested_frame);
	root["stroke"]["red"] = add_property_json("Red", stroke.red.GetValue(requested_frame), "float", "", &stroke.red, 0, 255, false, requested_frame);
	root["stroke"]["blue"] = add_property_json("Blue", stroke.blue.GetValue(requested_frame), "float", "", &stroke.blue, 0, 255, false, requested_frame);
	root["stroke"]["green"] = add_property_json("Green", stroke.green.GetValue(requested_frame), "float", "", &stroke.green, 0, 255, false, requested_frame);
	root["stroke_width"] = add_property_json("Stroke Width", stroke_width.GetValue(requested_frame), "int", "", &stroke_width, 1, 10, false, requested_frame);
	root["stroke_alpha"] = add_property_json("Stroke alpha", stroke_alpha.GetValue(requested_frame), "float", "", &stroke_alpha, 0.0, 1.0, false, requested_frame);

	root["background_alpha"] = add_property_json("Background Alpha", background_alpha.GetValue(requested_frame), "float", "", &background_alpha, 0.0, 1.0, false, requested_frame);
	root["background_corner"] = add_property_json("Background Corner Radius", background_corner.GetValue(requested_frame), "int", "", &background_corner, 0.0, 150.0, false, requested_frame);

	root["background"] = add_property_json("Background", 0.0, "color", "", NULL, 0, 255, false, requested_frame);
	root["background"]["red"] = add_property_json("Red", background.red.GetValue(requested_frame), "float", "", &background.red, 0, 255, false, requested_frame);
	root["background"]["blue"] = add_property_json("Blue", background.blue.GetValue(requested_frame), "float", "", &background.blue, 0, 255, false, requested_frame);
	root["background"]["green"] = add_property_json("Green", background.green.GetValue(requested_frame), "float", "", &background.green, 0, 255, false, requested_frame);

	// Return formatted string
	return root;
}


// Generate JSON for a property
Json::Value TrackedObjectBBox::add_property_json(std::string name, float value, std::string type, std::string memo, const Keyframe* keyframe, float min_value, float max_value, bool readonly, int64_t requested_frame) const {

	// Requested Point
	const Point requested_point(requested_frame, requested_frame);

	// Create JSON Object
	Json::Value prop = Json::Value(Json::objectValue);
	prop["name"] = name;
	prop["value"] = value;
	prop["memo"] = memo;
	prop["type"] = type;
	prop["min"] = min_value;
	prop["max"] = max_value;
	if (keyframe) {
		prop["keyframe"] = keyframe->Contains(requested_point);
		prop["points"] = int(keyframe->GetCount());
		Point closest_point = keyframe->GetClosestPoint(requested_point);
		prop["interpolation"] = closest_point.interpolation;
		prop["closest_point_x"] = closest_point.co.X;
		prop["previous_point_x"] = keyframe->GetPreviousPoint(closest_point).co.X;
	}
	else {
		prop["keyframe"] = false;
		prop["points"] = 0;
		prop["interpolation"] = CONSTANT;
		prop["closest_point_x"] = -1;
		prop["previous_point_x"] = -1;
	}

	prop["readonly"] = readonly;
	prop["choices"] = Json::Value(Json::arrayValue);

	// return JsonValue
	return prop;
}

// Return a map that contains the bounding box properties and it's keyframes indexed by their names
std::map<std::string, float> TrackedObjectBBox::GetBoxValues(int64_t frame_number) const {

	// Create the map
	std::map<std::string, float> boxValues;

	// Get bounding box of the current frame
	BBox box = GetBox(frame_number);

	// Save the bounding box properties
	boxValues["cx"] = box.cx;
	boxValues["cy"] = box.cy;
	boxValues["w"] = box.width;
	boxValues["h"] = box.height;
	boxValues["ang"] = box.angle;

	// Save the keyframes values
	boxValues["sx"] = this->scale_x.GetValue(frame_number);
	boxValues["sy"] = this->scale_y.GetValue(frame_number);
	boxValues["dx"] = this->delta_x.GetValue(frame_number);
	boxValues["dy"] = this->delta_y.GetValue(frame_number);
	return boxValues;
}
