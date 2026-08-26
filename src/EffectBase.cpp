/**
 * @file
 * @brief Source file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#include "EffectBase.h"

#include "Exceptions.h"
#include "Clip.h"
#include "Timeline.h"
#include "TrackedObjectBBox.h"
#include "ReaderBase.h"
#include "ChunkReader.h"
#include "FFmpegReader.h"
#include "QtImageReader.h"
#include "ZmqLogger.h"
#include <omp.h>
#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QRectF>

#ifdef USE_IMAGEMAGICK
	#include "ImageReader.h"
#endif

using namespace openshot;

// Initialize the values of the EffectInfo struct
void EffectBase::InitEffectInfo()
{
	// Init clip settings
	Position(0.0);
	Layer(0);
	Start(0.0);
	End(0.0);
	Order(0);
	ParentClip(NULL);
	parentEffect = NULL;
	mask_invert = false;
	mask_reader = NULL;
	mask_source_id = "";
	mask_time_mode = MASK_TIME_SOURCE_FPS;
	mask_loop_mode = MASK_LOOP_PLAY_ONCE;

	info.has_video = false;
	info.has_audio = false;
	info.has_tracked_object = false;
	info.name = "";
	info.description = "";
	info.parent_effect_id = "";
	info.apply_before_clip = true;
}

// Display file information
void EffectBase::DisplayInfo(std::ostream* out) {
	*out << std::fixed << std::setprecision(2) << std::boolalpha;
	*out << "----------------------------" << std::endl;
	*out << "----- Effect Information -----" << std::endl;
	*out << "----------------------------" << std::endl;
	*out << "--> Name: " << info.name << std::endl;
	*out << "--> Description: " << info.description << std::endl;
	*out << "--> Has Video: " << info.has_video << std::endl;
	*out << "--> Has Audio: " << info.has_audio << std::endl;
	*out << "--> Apply to Source: " << info.apply_before_clip << std::endl;
	*out << "--> Order: " << order << std::endl;
	*out << "----------------------------" << std::endl;
}

// Constrain a color value from 0 to 255
int EffectBase::constrain(int color_value)
{
	// Constrain new color from 0 to 255
	if (color_value < 0)
		color_value = 0;
	else if (color_value > 255)
		color_value = 255;

	return color_value;
}

// Generate JSON string of this object
std::string EffectBase::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value EffectBase::JsonValue() const {

	// Create root json object
	Json::Value root = ClipBase::JsonValue(); // get parent properties
	root["name"] = info.name;
	root["class_name"] = info.class_name;
	root["description"] = info.description;
	root["parent_effect_id"] = info.parent_effect_id;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;
	root["has_tracked_object"] = info.has_tracked_object;
	root["apply_before_clip"] = info.apply_before_clip;
	root["order"] = Order();
	root["mask_invert"] = mask_invert;
	root["mask_source_id"] = mask_source_id;
	root["mask_time_mode"] = mask_time_mode;
	root["mask_loop_mode"] = mask_loop_mode;
	if (mask_reader)
		root["mask_reader"] = mask_reader->JsonValue();
	else
		root["mask_reader"] = Json::objectValue;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void EffectBase::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		Json::Value root = openshot::stringToJson(value);
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
void EffectBase::SetJsonValue(const Json::Value root) {
	const std::string original_id = this->Id();
	const std::string original_parent_effect_id = this->info.parent_effect_id;

	// Set this effect properties with the parent effect properties (except the id and parent_effect_id)
	Json::Value my_root;
	const bool applying_parent_payload =
		!original_id.empty() &&
		!original_parent_effect_id.empty() &&
		!root["id"].isNull() &&
		root["id"].asString() == original_parent_effect_id &&
		root["id"].asString() != original_id;
	if (applying_parent_payload) {
		my_root = root;
		my_root["id"] = original_id;
		my_root["parent_effect_id"] = original_parent_effect_id;
	} else if (parentEffect){
		my_root = parentEffect->JsonValue();
		my_root["id"] = this->Id();
		my_root["parent_effect_id"] = this->info.parent_effect_id;
	} else {
		my_root = root;
	}

	// Legacy compatibility: older shared-mask JSON stored source trim
	// separately from the effect trim. Canonical trim now uses ClipBase.
	if (my_root["start"].isNull() && !my_root["mask_start"].isNull())
		my_root["start"] = my_root["mask_start"];
	if (my_root["end"].isNull() && !my_root["mask_end"].isNull())
		my_root["end"] = my_root["mask_end"];

	// Set parent data
	ClipBase::SetJsonValue(my_root);

	// Set data from Json (if key is found)
	if (!my_root["order"].isNull())
		Order(my_root["order"].asInt());

	if (!my_root["apply_before_clip"].isNull())
		info.apply_before_clip = my_root["apply_before_clip"].asBool();

	if (!my_root["mask_invert"].isNull())
		mask_invert = my_root["mask_invert"].asBool();
	if (!my_root["mask_source_id"].isNull())
		MaskSourceId(my_root["mask_source_id"].asString());
	if (!my_root["mask_time_mode"].isNull()) {
		const int time_mode = my_root["mask_time_mode"].asInt();
		mask_time_mode = (time_mode == MASK_TIME_TIMELINE || time_mode == MASK_TIME_SOURCE_FPS)
			? time_mode : MASK_TIME_SOURCE_FPS;
	}
	if (!my_root["mask_loop_mode"].isNull()) {
		const int loop_mode = my_root["mask_loop_mode"].asInt();
		if (loop_mode >= MASK_LOOP_PLAY_ONCE && loop_mode <= MASK_LOOP_PING_PONG)
			mask_loop_mode = loop_mode;
		else
			mask_loop_mode = MASK_LOOP_PLAY_ONCE;
	}

	const Json::Value mask_reader_json =
		!my_root["mask_reader"].isNull() ? my_root["mask_reader"] : my_root["reader"];

	if (!mask_reader_json.isNull()) {
		if (!mask_reader_json["type"].isNull()) {
			MaskReader(CreateReaderFromJson(mask_reader_json));
		} else if (mask_reader_json.isObject() && mask_reader_json.empty()) {
			MaskReader(NULL);
		}
	}

	if (!my_root["parent_effect_id"].isNull()){
		info.parent_effect_id = my_root["parent_effect_id"].asString();
		if (info.parent_effect_id.size() > 0 && info.parent_effect_id != "" && parentEffect == NULL)
			SetParentEffect(info.parent_effect_id);
		else
			parentEffect = NULL;
	}

	if (ParentTimeline()){
		// Get parent timeline
		Timeline* parentTimeline = static_cast<Timeline *>(ParentTimeline());

		// Get the list of effects on the timeline
		std::list<EffectBase*> effects = parentTimeline->ClipEffects();

		// TODO: Fix recursive call for Object Detection

		// Loop through the effects and check if we have a child effect linked to this effect
		for (auto const& effect : effects){
			// Set the properties of all effects which parentEffect points to this
			if ((effect->info.parent_effect_id == this->Id()) && (effect->Id() != this->Id()))
				effect->SetJsonValue(my_root);
		}
	}
}

// Generate Json::Value for this object
Json::Value EffectBase::JsonInfo() const {

	// Create root json object
	Json::Value root;
	root["name"] = info.name;
	root["class_name"] = info.class_name;
	root["description"] = info.description;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;

	// return JsonValue
	return root;
}

// Get all properties for a specific frame
Json::Value EffectBase::BasePropertiesJSON(int64_t requested_frame) const {
	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);

	// Add replace_image choices (dropdown style)
	root["apply_before_clip"] = add_property_json("Apply to Source", info.apply_before_clip, "int", "", NULL, 0, 1, false, requested_frame);
	root["apply_before_clip"]["choices"].append(add_property_choice_json("Yes", true, info.apply_before_clip));
	root["apply_before_clip"]["choices"].append(add_property_choice_json("No", false, info.apply_before_clip));

	// Set the parent effect which properties this effect will inherit
	root["parent_effect_id"] = add_property_json("Parent", 0.0, "string", info.parent_effect_id, NULL, -1, -1, false, requested_frame);

	if (info.has_video) {
		root["mask_invert"] = add_property_json("Mask: Invert", mask_invert, "int", "", NULL, 0, 1, false, requested_frame);
		root["mask_invert"]["choices"].append(add_property_choice_json("Yes", true, mask_invert));
		root["mask_invert"]["choices"].append(add_property_choice_json("No", false, mask_invert));

		root["mask_time_mode"] = add_property_json("Mask: Time Mode", mask_time_mode, "int", "", NULL, 0, 1, false, requested_frame);
		root["mask_time_mode"]["choices"].append(add_property_choice_json("Timeline", MASK_TIME_TIMELINE, mask_time_mode));
		root["mask_time_mode"]["choices"].append(add_property_choice_json("Source FPS", MASK_TIME_SOURCE_FPS, mask_time_mode));

		root["mask_loop_mode"] = add_property_json("Mask: Loop", mask_loop_mode, "int", "", NULL, 0, 2, false, requested_frame);
		root["mask_loop_mode"]["choices"].append(add_property_choice_json("Play Once", MASK_LOOP_PLAY_ONCE, mask_loop_mode));
		root["mask_loop_mode"]["choices"].append(add_property_choice_json("Repeat", MASK_LOOP_REPEAT, mask_loop_mode));
		root["mask_loop_mode"]["choices"].append(add_property_choice_json("Ping-Pong", MASK_LOOP_PING_PONG, mask_loop_mode));

		if (mask_reader)
			root["mask_reader"] = add_property_json("Mask: Source", 0.0, "reader", mask_reader->Json(), NULL, 0, 1, false, requested_frame);
		else
			root["mask_reader"] = add_property_json("Mask: Source", 0.0, "reader", "{}", NULL, 0, 1, false, requested_frame);

		root["mask_source_id"] = add_property_json("Mask: Effect Source", 0.0, "string", mask_source_id, NULL, -1, -1, false, requested_frame);
	}

	return root;
}

ReaderBase* EffectBase::CreateReaderFromJson(const Json::Value& reader_json) const {
	if (reader_json["type"].isNull())
		return NULL;

	ReaderBase* reader = NULL;
	const std::string type = reader_json["type"].asString();

	if (type == "FFmpegReader") {
		reader = new FFmpegReader(reader_json["path"].asString());
		reader->SetJsonValue(reader_json);
		// Mask readers are video-only sources. Disabling audio avoids FFmpeg
		// A/V readiness fallbacks that can repeat stale video frames.
		reader->info.has_audio = false;
		reader->info.audio_stream_index = -1;
	} else if (type == "QtImageReader") {
		reader = new QtImageReader(reader_json["path"].asString());
		reader->SetJsonValue(reader_json);
	} else if (type == "ChunkReader") {
		reader = new ChunkReader(reader_json["path"].asString(),
								 static_cast<ChunkVersion>(reader_json["chunk_version"].asInt()));
		reader->SetJsonValue(reader_json);
#ifdef USE_IMAGEMAGICK
	} else if (type == "ImageReader") {
		reader = new ImageReader(reader_json["path"].asString());
		reader->SetJsonValue(reader_json);
#endif
	}

	return reader;
}

void EffectBase::MaskReader(ReaderBase* new_reader) {
	if (mask_reader == new_reader)
		return;

	if (mask_reader) {
		mask_reader->Close();
		delete mask_reader;
	}

	mask_reader = new_reader;
	cached_single_mask_image.reset();
	cached_single_mask_width = 0;
	cached_single_mask_height = 0;
	if (mask_reader)
		mask_reader->ParentClip(clip);
}

void EffectBase::MaskSourceId(const std::string& new_mask_source_id) {
	mask_source_id = new_mask_source_id;
	cached_single_mask_image.reset();
	cached_single_mask_width = 0;
	cached_single_mask_height = 0;
}

EffectBase* EffectBase::ResolveMaskSourceEffect() {
	if (mask_source_id.empty())
		return NULL;

	Clip* parent_clip = dynamic_cast<Clip*>(clip);
	if (parent_clip) {
		EffectBase* source = parent_clip->GetEffect(mask_source_id);
		if (source && source != this)
			return source;
	}

	Timeline* parent_timeline = dynamic_cast<Timeline*>(ParentTimeline());
	if (parent_timeline) {
		EffectBase* source = parent_timeline->GetClipEffect(mask_source_id);
		if (!source)
			source = parent_timeline->GetEffect(mask_source_id);
		if (source && source != this)
			return source;
	}

	return NULL;
}

double EffectBase::ResolveMaskHostFps() {
	if (clip) {
		Clip* parent_clip = dynamic_cast<Clip*>(clip);
		if (parent_clip && parent_clip->info.fps.num > 0 && parent_clip->info.fps.den > 0)
			return parent_clip->info.fps.ToDouble();
	}

	Timeline* parent_timeline = dynamic_cast<Timeline*>(ParentTimeline());
	if (parent_timeline && parent_timeline->info.fps.num > 0 && parent_timeline->info.fps.den > 0)
		return parent_timeline->info.fps.ToDouble();

	if (mask_reader && mask_reader->info.fps.num > 0 && mask_reader->info.fps.den > 0)
		return mask_reader->info.fps.ToDouble();

	return 30.0;
}

double EffectBase::ResolveMaskSourceDuration() const {
	if (!mask_reader)
		return 0.0;

	if (mask_reader->info.duration > 0.0f)
		return static_cast<double>(mask_reader->info.duration);

	if (mask_reader->info.video_length > 0 &&
		mask_reader->info.fps.num > 0 && mask_reader->info.fps.den > 0) {
		return static_cast<double>(mask_reader->info.video_length) / mask_reader->info.fps.ToDouble();
	}

	return 0.0;
}

int64_t EffectBase::MapMaskFrameNumber(int64_t frame_number) {
	if (!mask_reader)
		return frame_number;

	int64_t requested_index = std::max(int64_t(0), frame_number - 1);
	if (!clip && ParentTimeline()) {
		const double host_fps = ResolveMaskHostFps();
		if (host_fps > 0.0) {
			const int64_t start_offset = static_cast<int64_t>(std::llround(std::max(0.0f, Start()) * host_fps));
			requested_index = std::max(int64_t(0), requested_index - start_offset);
		}
	}
	int64_t mapped_index = requested_index;

	if (mask_time_mode == MASK_TIME_SOURCE_FPS &&
		mask_reader->info.fps.num > 0 && mask_reader->info.fps.den > 0) {
		const double host_fps = ResolveMaskHostFps();
		const double source_fps = mask_reader->info.fps.ToDouble();
		if (host_fps > 0.0 && source_fps > 0.0) {
			const double seconds = static_cast<double>(requested_index) / host_fps;
			mapped_index = static_cast<int64_t>(std::llround(seconds * source_fps));
		}
	}

	const int64_t source_len = mask_reader->info.video_length;
	const double source_fps = (mask_reader->info.fps.num > 0 && mask_reader->info.fps.den > 0)
		? mask_reader->info.fps.ToDouble() : 30.0;
	const double source_duration = ResolveMaskSourceDuration();
	const double start_sec = std::min<double>(std::max(0.0f, Start()), source_duration);
	const double end_sec = std::min<double>(std::max(0.0f, End()), source_duration);

	const int64_t range_start = std::max(int64_t(1), static_cast<int64_t>(std::llround(start_sec * source_fps)) + 1);
	int64_t range_end = (end_sec > 0.0)
		? static_cast<int64_t>(std::llround(end_sec * source_fps)) + 1
		: source_len;
	if (source_len > 0)
		range_end = std::min(range_end, source_len);
	if (range_end < range_start)
		range_end = range_start;

	const int64_t range_len = std::max(int64_t(1), range_end - range_start + 1);
	int64_t range_index = mapped_index;

	switch (mask_loop_mode) {
	case MASK_LOOP_REPEAT:
		range_index = mapped_index % range_len;
		break;
	case MASK_LOOP_PING_PONG:
		if (range_len > 1) {
			const int64_t cycle_len = (range_len * 2) - 2;
			int64_t phase = mapped_index % cycle_len;
			if (phase >= range_len)
				phase = cycle_len - phase;
			range_index = phase;
		} else {
			range_index = 0;
		}
		break;
	case MASK_LOOP_PLAY_ONCE:
	default:
		if (mapped_index < 0)
			range_index = 0;
		else if (mapped_index >= range_len)
			range_index = range_len - 1;
		else
			range_index = mapped_index;
		break;
	}

	int64_t mapped_frame = range_start + range_index;
	if (source_len > 0)
		mapped_frame = std::min(std::max(int64_t(1), mapped_frame), source_len);
	return std::max(int64_t(1), mapped_frame);
}

std::shared_ptr<QImage> EffectBase::GetMaskImage(std::shared_ptr<QImage> target_image, int64_t frame_number) {
	if (!target_image || target_image->isNull())
		return {};

	EffectBase* source_effect = ResolveMaskSourceEffect();
	if (source_effect) {
		auto generated_mask = source_effect->TrackedObjectMask(target_image, frame_number);
		if (generated_mask && !generated_mask->isNull())
			return generated_mask;

		auto empty_mask = std::make_shared<QImage>(
			target_image->width(), target_image->height(), QImage::Format_RGBA8888_Premultiplied);
		empty_mask->fill(QColor(0, 0, 0, 255));
		return empty_mask;
	}

	if (!mask_reader)
		return {};

	std::shared_ptr<QImage> source_mask;
	bool used_cached_scaled = false;
	#pragma omp critical (open_effect_mask_reader)
	{
		try {
			if (!mask_reader->IsOpen())
				mask_reader->Open();

			if (mask_reader->info.has_single_image &&
				cached_single_mask_image &&
				cached_single_mask_width == target_image->width() &&
				cached_single_mask_height == target_image->height()) {
				source_mask = cached_single_mask_image;
				used_cached_scaled = true;
			}
			else {
				const int64_t mapped_frame = MapMaskFrameNumber(frame_number);
				auto source_frame = mask_reader->GetFrame(mapped_frame);
				if (source_frame && source_frame->GetImage() && !source_frame->GetImage()->isNull())
					source_mask = std::make_shared<QImage>(*source_frame->GetImage());
			}
		} catch (const std::exception& e) {
			ZmqLogger::Instance()->Log(
				std::string("EffectBase::GetMaskImage unable to read mask frame: ") + e.what());
			source_mask.reset();
		}
	}

	if (!source_mask || source_mask->isNull())
		return {};

	if (used_cached_scaled)
		return source_mask;

	auto scaled_mask = std::make_shared<QImage>(
		source_mask->scaled(
			target_image->width(), target_image->height(),
			Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	if (mask_reader->info.has_single_image) {
		cached_single_mask_image = scaled_mask;
		cached_single_mask_width = target_image->width();
		cached_single_mask_height = target_image->height();
	}
	return scaled_mask;
}

std::shared_ptr<QImage> EffectBase::TrackedObjectMask(std::shared_ptr<QImage> target_image, int64_t frame_number) const {
#ifndef USE_OPENCV
	// Tracked-object bounding boxes are only produced by OpenCV-based
	// trackers; without OpenCV there is nothing to draw a mask for.
	(void)frame_number;
	return {};
#else
	if (!target_image || target_image->isNull() || trackedObjects.empty())
		return {};

	auto mask_image = std::make_shared<QImage>(
		target_image->width(), target_image->height(), QImage::Format_RGBA8888_Premultiplied);
	mask_image->fill(QColor(0, 0, 0, 255));

	QPainter painter(mask_image.get());
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QBrush(QColor(255, 255, 255, 255)));

	bool drew_any_box = false;
	for (auto const& trackedObject : trackedObjects) {
		auto bbox = std::dynamic_pointer_cast<TrackedObjectBBox>(trackedObject.second);
		if (!bbox)
			continue;
		if (!bbox->Contains(frame_number) || bbox->visible.GetValue(frame_number) != 1)
			continue;

		BBox box = bbox->GetBox(frame_number);
		if (box.width <= 0.0f || box.height <= 0.0f || box.cx < 0.0f || box.cy < 0.0f)
			continue;

		const double x = (box.cx - box.width / 2.0) * target_image->width();
		const double y = (box.cy - box.height / 2.0) * target_image->height();
		const double w = box.width * target_image->width();
		const double h = box.height * target_image->height();
		const double corner = bbox->background_corner.GetValue(frame_number);
		QRectF rect(x, y, w, h);

		if (std::abs(box.angle) > 0.0001f) {
			painter.save();
			painter.translate(rect.center());
			painter.rotate(box.angle);
			painter.drawRoundedRect(QRectF(-w / 2.0, -h / 2.0, w, h), corner, corner);
			painter.restore();
		} else {
			painter.drawRoundedRect(rect, corner, corner);
		}
		drew_any_box = true;
	}

	painter.end();
	if (!drew_any_box)
		return {};
	return mask_image;
#endif
}

void EffectBase::BlendWithMask(std::shared_ptr<QImage> original_image, std::shared_ptr<QImage> effected_image,
							   std::shared_ptr<QImage> mask_image) const {
	if (!original_image || !effected_image || !mask_image)
		return;
	if (original_image->size() != effected_image->size() || effected_image->size() != mask_image->size())
		return;

	unsigned char* original_pixels = reinterpret_cast<unsigned char*>(original_image->bits());
	unsigned char* effected_pixels = reinterpret_cast<unsigned char*>(effected_image->bits());
	unsigned char* mask_pixels = reinterpret_cast<unsigned char*>(mask_image->bits());
	const int width = effected_image->width();
	const int height = effected_image->height();
	const int original_stride = original_image->bytesPerLine();
	const int effected_stride = effected_image->bytesPerLine();
	const int mask_stride = mask_image->bytesPerLine();

	#pragma omp parallel for schedule(static)
	for (int y = 0; y < height; ++y) {
		unsigned char* original_row = original_pixels + y * original_stride;
		unsigned char* effected_row = effected_pixels + y * effected_stride;
		unsigned char* mask_row = mask_pixels + y * mask_stride;
		for (int x = 0; x < width; ++x) {
			const int idx = x * 4;
			int gray = qGray(mask_row[idx], mask_row[idx + 1], mask_row[idx + 2]);
			if (mask_invert)
				gray = 255 - gray;
			const float factor = static_cast<float>(gray) / 255.0f;
			const float inverse = 1.0f - factor;

			effected_row[idx] = static_cast<unsigned char>(
				(original_row[idx] * inverse) + (effected_row[idx] * factor));
			effected_row[idx + 1] = static_cast<unsigned char>(
				(original_row[idx + 1] * inverse) + (effected_row[idx + 1] * factor));
			effected_row[idx + 2] = static_cast<unsigned char>(
				(original_row[idx + 2] * inverse) + (effected_row[idx + 2] * factor));
			effected_row[idx + 3] = static_cast<unsigned char>(
				(original_row[idx + 3] * inverse) + (effected_row[idx + 3] * factor));
		}
	}
}

std::shared_ptr<openshot::Frame> EffectBase::ProcessFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) {
	// Audio-only effects skip common mask handling.
	if (!info.has_video || (!mask_reader && mask_source_id.empty()))
		return GetFrame(frame, frame_number);

	// Effects that already apply masks inside GetFrame() should bypass common blend handling.
	if (HandlesMaskInternally())
		return GetFrame(frame, frame_number);

	auto pre_image = frame->GetImage();
	if (!pre_image || pre_image->isNull())
		return GetFrame(frame, frame_number);

	const auto original_image = std::make_shared<QImage>(pre_image->copy());
	auto output_frame = GetFrame(frame, frame_number);
	if (!output_frame)
		return output_frame;
	auto effected_image = output_frame->GetImage();
	if (!effected_image || effected_image->isNull() ||
		effected_image->size() != original_image->size())
		return output_frame;

	auto mask_image = GetMaskImage(effected_image, frame_number);
	if (!mask_image || mask_image->isNull())
		return output_frame;

	if (UseCustomMaskBlend(frame_number))
		ApplyCustomMaskBlend(original_image, effected_image, mask_image, frame_number);
	else
		BlendWithMask(original_image, effected_image, mask_image);

	return output_frame;
}

/// Parent clip object of this reader (which can be unparented and NULL)
openshot::ClipBase* EffectBase::ParentClip() {
	return clip;
}

/// Set parent clip object of this reader
void EffectBase::ParentClip(openshot::ClipBase* new_clip) {
	clip = new_clip;
	if (mask_reader)
		mask_reader->ParentClip(new_clip);
}

// Set the parent effect from which this properties will be set to
void EffectBase::SetParentEffect(std::string parentEffect_id) {

	// Get parent Timeline
	Timeline* parentTimeline = static_cast<Timeline *>(ParentTimeline());

	if (parentTimeline){

		// Get a pointer to the parentEffect
		EffectBase* parentEffectPtr = parentTimeline->GetClipEffect(parentEffect_id);

		if (parentEffectPtr){
			// Set the parent Effect
			parentEffect = parentEffectPtr;

			// Set the properties of this effect with the parent effect's properties
			Json::Value EffectJSON = parentEffect->JsonValue();
			EffectJSON["id"] = this->Id();
			EffectJSON["parent_effect_id"] = this->info.parent_effect_id;
			this->SetJsonValue(EffectJSON);
		}
	}
	return;
}

// Return the ID of this effect's parent clip
std::string EffectBase::ParentClipId() const{
	if(clip)
		return clip->Id();
	else
		return "";
}

EffectBase::~EffectBase() {
	MaskReader(NULL);
}
