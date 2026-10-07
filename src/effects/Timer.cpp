/**
 * @file
 * @brief Source file for Timer effect class
 * @author OpenShot Studios, LLC
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Timer.h"
#include "Exceptions.h"
#include "../Clip.h"
#include "../Timeline.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>

using namespace openshot;

namespace {
	int ClampGravity(int gravity)
	{
		if (gravity < GRAVITY_TOP_LEFT || gravity > GRAVITY_BOTTOM_RIGHT)
			return GRAVITY_BOTTOM;
		return gravity;
	}

	double TextAdvance(const QFontMetricsF& metrics, const QString& text)
	{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		return metrics.horizontalAdvance(text);
#else
		return metrics.width(text);
#endif
	}
}

Timer::Timer() :
	mode(TIMER_MODE_COUNT_UP),
	time_source(TIMER_TIME_SOURCE),
	format(TIMER_FORMAT_MM_SS),
	clamp(1),
	gravity(GRAVITY_BOTTOM),
	show_background(1),
	font_name("sans"),
	prefix(""),
	suffix(""),
	color("#ffffff"),
	stroke("#000000"),
	background("#000000"),
	start_time(0.0),
	end_time(0.0),
	font_size(48.0),
	font_alpha(1.0),
	stroke_width(2.0),
	x_offset(0.0),
	y_offset(0.0),
	background_alpha(0.45),
	background_padding(14.0),
	background_corner(6.0)
{
	init_effect_details();
}

void Timer::init_effect_details()
{
	InitEffectInfo();
	info.class_name = "Timer";
	info.name = "Timer";
	info.description = "Render a styled count up, count down, clock, timecode, or frame number overlay.";
	info.has_audio = false;
	info.has_video = true;
}

double Timer::ResolveFps() const
{
	Clip* clip = (Clip*) const_cast<Timer*>(this)->ParentClip();
	Timeline* timeline = NULL;

	if (clip && clip->ParentTimeline() != NULL) {
		timeline = (Timeline*) clip->ParentTimeline();
	} else if (const_cast<Timer*>(this)->ParentTimeline() != NULL) {
		timeline = (Timeline*) const_cast<Timer*>(this)->ParentTimeline();
	}

	if (time_source == TIMER_TIME_SOURCE && clip != NULL && clip->Reader() != NULL && clip->Reader()->info.fps.ToDouble() > 0.0)
		return clip->Reader()->info.fps.ToDouble();
	if (timeline != NULL && timeline->info.fps.ToDouble() > 0.0)
		return timeline->info.fps.ToDouble();
	if (clip != NULL && clip->Reader() != NULL && clip->Reader()->info.fps.ToDouble() > 0.0)
		return clip->Reader()->info.fps.ToDouble();
	return 30.0;
}

int64_t Timer::EffectiveFrameNumber(int64_t frame_number) const
{
	if (time_source != TIMER_TIME_SOURCE)
		return std::max<int64_t>(1, frame_number);

	Clip* clip = (Clip*) const_cast<Timer*>(this)->ParentClip();
	if (clip != NULL && clip->time.GetLength() > 1)
		return std::max<int64_t>(1, clip->time.GetLong(frame_number));

	return std::max<int64_t>(1, frame_number);
}

double Timer::CountdownDuration(int64_t frame_number) const
{
	const double configured_duration = end_time.GetValue(frame_number);
	if (configured_duration > 0.0)
		return configured_duration;

	Clip* clip = (Clip*) const_cast<Timer*>(this)->ParentClip();
	if (clip != NULL)
		return std::max(0.0f, clip->End() - clip->Start());

	return 0.0;
}

std::string Timer::FormatSeconds(double seconds, double fps, bool duration_style) const
{
	if (duration_style && clamp && seconds < 0.0)
		seconds = 0.0;

	const bool negative = seconds < 0.0;
	const int64_t total_milliseconds = std::llround(std::fabs(seconds) * 1000.0);
	const int milliseconds = static_cast<int>(total_milliseconds % 1000);
	const int64_t total_seconds = total_milliseconds / 1000;
	const int hours = static_cast<int>(total_seconds / 3600);
	const int minutes = static_cast<int>((total_seconds / 60) % 60);
	const int whole_seconds = static_cast<int>(total_seconds % 60);

	std::ostringstream text;
	if (negative)
		text << "-";

	if (format == TIMER_FORMAT_HH_MM_SS_MILLISECONDS) {
		text << std::setfill('0') << std::setw(2) << hours << ":"
			 << std::setw(2) << minutes << ":"
			 << std::setw(2) << whole_seconds << "."
			 << std::setw(3) << milliseconds;
	} else if (format == TIMER_FORMAT_HH_MM_SS) {
		text << std::setfill('0') << std::setw(2) << hours << ":"
			 << std::setw(2) << minutes << ":"
			 << std::setw(2) << whole_seconds;
	} else if (format == TIMER_FORMAT_TIMECODE) {
		text << FormatTimecode(seconds, fps);
	} else if (format == TIMER_FORMAT_FRAMES) {
		text << static_cast<int64_t>(std::llround(seconds * fps));
	} else {
		const int total_minutes = hours * 60 + minutes;
		text << std::setfill('0') << std::setw(2) << total_minutes << ":"
			 << std::setw(2) << whole_seconds;
	}

	return text.str();
}

std::string Timer::FormatTimecode(double seconds, double fps) const
{
	const bool negative = seconds < 0.0;
	const int64_t total_frames = std::llround(std::fabs(seconds) * fps);
	const int64_t rounded_fps = std::max<int64_t>(1, std::llround(fps));
	const int64_t frames = total_frames % rounded_fps;
	const int64_t total_seconds = total_frames / rounded_fps;
	const int64_t secs = total_seconds % 60;
	const int64_t mins = (total_seconds / 60) % 60;
	const int64_t hours = total_seconds / 3600;

	std::ostringstream text;
	if (negative)
		text << "-";
	text << std::setfill('0') << std::setw(2) << hours << ":"
		 << std::setw(2) << mins << ":"
		 << std::setw(2) << secs << ":"
		 << std::setw(2) << frames;
	return text.str();
}

double Timer::TimerSeconds(int64_t frame_number) const
{
	const double fps = ResolveFps();
	const int64_t effective_frame = EffectiveFrameNumber(frame_number);
	const double elapsed = static_cast<double>(effective_frame - 1) / fps;

	if (mode == TIMER_MODE_COUNT_DOWN)
		return CountdownDuration(frame_number) - start_time.GetValue(frame_number) - elapsed;

	return start_time.GetValue(frame_number) + elapsed;
}

std::string Timer::TimerText(int64_t frame_number) const
{
	const double fps = ResolveFps();
	const int64_t effective_frame = EffectiveFrameNumber(frame_number);
	std::ostringstream text;
	text << prefix;

	if (mode == TIMER_MODE_FRAME_NUMBER || format == TIMER_FORMAT_FRAMES) {
		const int64_t frame_offset = std::llround(start_time.GetValue(frame_number) * fps);
		text << (effective_frame + frame_offset);
	} else if (mode == TIMER_MODE_TIMECODE || format == TIMER_FORMAT_TIMECODE) {
		text << FormatTimecode(TimerSeconds(frame_number), fps);
	} else if (mode == TIMER_MODE_CLOCK) {
		double seconds = std::fmod(TimerSeconds(frame_number), 24.0 * 60.0 * 60.0);
		if (seconds < 0.0)
			seconds += 24.0 * 60.0 * 60.0;
		text << FormatSeconds(seconds, fps, false);
	} else {
		text << FormatSeconds(TimerSeconds(frame_number), fps, true);
	}

	text << suffix;
	return text.str();
}

std::string Timer::TimerLayoutText(int64_t frame_number) const
{
	if (mode == TIMER_MODE_FRAME_NUMBER || format == TIMER_FORMAT_FRAMES)
		return TimerText(frame_number);

	std::string timer_template;
	if (mode == TIMER_MODE_TIMECODE || format == TIMER_FORMAT_TIMECODE)
		timer_template = "88:88:88:88";
	else if (format == TIMER_FORMAT_HH_MM_SS_MILLISECONDS)
		timer_template = "88:88:88.888";
	else if (format == TIMER_FORMAT_HH_MM_SS)
		timer_template = "88:88:88";
	else
		timer_template = "88:88";

	return prefix + timer_template + suffix;
}

std::shared_ptr<openshot::Frame> Timer::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	Clip* clip = (Clip*) ParentClip();
	Timeline* timeline = NULL;
	QSize image_size(1280, 720);

	if (clip && clip->ParentTimeline() != NULL) {
		timeline = (Timeline*) clip->ParentTimeline();
	} else if (this->ParentTimeline() != NULL) {
		timeline = (Timeline*) this->ParentTimeline();
	}

	if (timeline != NULL) {
		image_size = QSize(timeline->info.width, timeline->info.height);
	} else if (clip != NULL && clip->Reader() != NULL) {
		image_size = QSize(clip->Reader()->info.width, clip->Reader()->info.height);
	}

	if (!frame->has_image_data)
		frame->AddColor(image_size.width(), image_size.height(), "#00000000");

	std::shared_ptr<QImage> frame_image = frame->GetImage();
	if (!frame_image || frame_image->isNull())
		return frame;

	const double scale_factor = frame_image->width() / 600.0;
	const double font_size_value = std::max(1.0, font_size.GetValue(frame_number) * scale_factor);
	const double stroke_width_value = std::max(0.0, stroke_width.GetValue(frame_number) * scale_factor);
	const double padding_value = std::max(0.0, background_padding.GetValue(frame_number) * scale_factor);
	const double corner_value = std::max(0.0, background_corner.GetValue(frame_number) * scale_factor);
	const QString timer_text = QString::fromStdString(TimerText(frame_number));
	const QString layout_text = QString::fromStdString(TimerLayoutText(frame_number));

	QFont font(QString::fromStdString(font_name), int(font_size_value));
	font.setPixelSize(font_size_value);
	QFontMetricsF metrics(font);
	QRectF text_bounds = metrics.tightBoundingRect(timer_text);
	if (text_bounds.isEmpty())
		text_bounds = metrics.boundingRect(timer_text);
	QRectF layout_bounds = metrics.tightBoundingRect(layout_text);
	if (layout_bounds.isEmpty())
		layout_bounds = metrics.boundingRect(layout_text);

	const double text_width = std::max(1.0, text_bounds.width());
	const double text_height = std::max(1.0, text_bounds.height());
	double digit_slot_width = 0.0;
	for (int digit = 0; digit <= 9; ++digit)
		digit_slot_width = std::max(digit_slot_width, TextAdvance(metrics, QString::number(digit)));

	const bool use_slot_layout = timer_text.size() == layout_text.size() &&
		!(mode == TIMER_MODE_FRAME_NUMBER || format == TIMER_FORMAT_FRAMES);
	std::vector<double> slot_widths;
	double layout_width = 0.0;
	if (use_slot_layout) {
		slot_widths.reserve(layout_text.size());
		for (int index = 0; index < layout_text.size(); ++index) {
			const QString layout_char(layout_text[index]);
			const double slot_width = layout_text[index].isDigit() ? digit_slot_width : TextAdvance(metrics, layout_char);
			slot_widths.push_back(slot_width);
			layout_width += slot_width;
		}
	} else {
		layout_width = std::max(text_width, layout_bounds.width());
	}
	layout_width = std::max(1.0, layout_width);
	const double rendered_box_width = layout_width + (padding_value * 2.0);
	const double box_height = std::max(text_height, layout_bounds.height()) + (padding_value * 2.0);

	double x = padding_value;
	double y = 0.0;
	const int resolved_gravity = ClampGravity(gravity);
	if (resolved_gravity == GRAVITY_TOP || resolved_gravity == GRAVITY_CENTER || resolved_gravity == GRAVITY_BOTTOM)
		x = (frame_image->width() - rendered_box_width) / 2.0;
	else if (resolved_gravity == GRAVITY_TOP_RIGHT || resolved_gravity == GRAVITY_RIGHT || resolved_gravity == GRAVITY_BOTTOM_RIGHT)
		x = frame_image->width() - rendered_box_width - padding_value;

	switch (resolved_gravity) {
	case GRAVITY_TOP_LEFT:
	case GRAVITY_TOP:
	case GRAVITY_TOP_RIGHT:
		break;
	case GRAVITY_LEFT:
	case GRAVITY_CENTER:
	case GRAVITY_RIGHT:
		y = (frame_image->height() - box_height) / 2.0;
		break;
	case GRAVITY_BOTTOM_LEFT:
	case GRAVITY_BOTTOM:
	case GRAVITY_BOTTOM_RIGHT:
		y = frame_image->height() - box_height;
		break;
	default:
		break;
	}
	x += frame_image->width() * (x_offset.GetValue(frame_number) / 100.0);
	y += frame_image->height() * (y_offset.GetValue(frame_number) / 100.0);
	x = std::round(x);
	y = std::round(y);

	double text_x = x + padding_value - text_bounds.left();
	if (resolved_gravity == GRAVITY_TOP || resolved_gravity == GRAVITY_CENTER || resolved_gravity == GRAVITY_BOTTOM)
		text_x = x + padding_value + ((layout_width - text_width) / 2.0) - text_bounds.left();
	else if (resolved_gravity == GRAVITY_TOP_RIGHT || resolved_gravity == GRAVITY_RIGHT || resolved_gravity == GRAVITY_BOTTOM_RIGHT)
		text_x = x + padding_value + layout_width - text_width - text_bounds.left();
	text_x = std::round(text_x);
	const double text_y = std::round(y + padding_value - text_bounds.top());

	QPainter painter(frame_image.get());
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing, true);
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

	if (show_background) {
		QColor background_qcolor(QString::fromStdString(background.GetColorHex(frame_number)));
		background_qcolor.setAlphaF(std::max(0.0, std::min(1.0, background_alpha.GetValue(frame_number))));
		painter.setPen(Qt::NoPen);
		painter.setBrush(QBrush(background_qcolor));
		painter.drawRoundedRect(QRectF(x, y, rendered_box_width, box_height), corner_value, corner_value);
	}

	QPainterPath path;
	if (use_slot_layout) {
		double cursor_x = x + padding_value;
		for (int index = 0; index < timer_text.size(); ++index) {
			const QString text_char(timer_text[index]);
			QRectF char_bounds = metrics.tightBoundingRect(text_char);
			if (char_bounds.isEmpty())
				char_bounds = metrics.boundingRect(text_char);

			const double char_x = std::round(cursor_x);
			path.addText(QPointF(char_x - char_bounds.left(), text_y), font, text_char);
			cursor_x += slot_widths[index];
		}
	} else {
		path.addText(QPointF(text_x, text_y), font, timer_text);
	}

	QColor stroke_qcolor(QString::fromStdString(stroke.GetColorHex(frame_number)));
	stroke_qcolor.setAlphaF(std::max(0.0, std::min(1.0, font_alpha.GetValue(frame_number))));
	QPen pen(stroke_qcolor);
	pen.setWidthF(stroke_width_value);
	painter.setPen(stroke_width_value <= 0.0 ? Qt::NoPen : pen);

	QColor font_qcolor(QString::fromStdString(color.GetColorHex(frame_number)));
	font_qcolor.setAlphaF(std::max(0.0, std::min(1.0, font_alpha.GetValue(frame_number))));
	painter.setBrush(QBrush(font_qcolor));
	painter.drawPath(path);
	painter.end();

	return frame;
}

std::string Timer::Json() const
{
	return JsonValue().toStyledString();
}

Json::Value Timer::JsonValue() const
{
	Json::Value root = EffectBase::JsonValue();
	root["type"] = info.class_name;
	root["mode"] = mode;
	root["time_source"] = time_source;
	root["format"] = format;
	root["clamp"] = clamp;
	root["gravity"] = ClampGravity(gravity);
	root["show_background"] = show_background;
	root["font_name"] = font_name;
	root["prefix"] = prefix;
	root["suffix"] = suffix;
	root["color"] = color.JsonValue();
	root["stroke"] = stroke.JsonValue();
	root["background"] = background.JsonValue();
	root["start_time"] = start_time.JsonValue();
	root["end_time"] = end_time.JsonValue();
	root["font_size"] = font_size.JsonValue();
	root["font_alpha"] = font_alpha.JsonValue();
	root["stroke_width"] = stroke_width.JsonValue();
	root["x_offset"] = x_offset.JsonValue();
	root["y_offset"] = y_offset.JsonValue();
	root["background_alpha"] = background_alpha.JsonValue();
	root["background_padding"] = background_padding.JsonValue();
	root["background_corner"] = background_corner.JsonValue();
	return root;
}

void Timer::SetJson(const std::string value)
{
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void Timer::SetJsonValue(const Json::Value root)
{
	EffectBase::SetJsonValue(root);

	if (!root["mode"].isNull())
		mode = root["mode"].asInt();
	if (!root["time_source"].isNull())
		time_source = root["time_source"].asInt();
	if (!root["format"].isNull())
		format = root["format"].asInt();
	if (!root["clamp"].isNull())
		clamp = root["clamp"].asInt();
	if (!root["gravity"].isNull())
		gravity = ClampGravity(root["gravity"].asInt());
	if (!root["show_background"].isNull())
		show_background = root["show_background"].asInt();
	if (!root["font_name"].isNull())
		font_name = root["font_name"].asString();
	if (!root["prefix"].isNull())
		prefix = root["prefix"].asString();
	if (!root["suffix"].isNull())
		suffix = root["suffix"].asString();
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
	if (!root["stroke"].isNull())
		stroke.SetJsonValue(root["stroke"]);
	if (!root["background"].isNull())
		background.SetJsonValue(root["background"]);
	if (!root["start_time"].isNull())
		start_time.SetJsonValue(root["start_time"]);
	if (!root["end_time"].isNull())
		end_time.SetJsonValue(root["end_time"]);
	if (!root["font_size"].isNull())
		font_size.SetJsonValue(root["font_size"]);
	if (!root["font_alpha"].isNull())
		font_alpha.SetJsonValue(root["font_alpha"]);
	if (!root["stroke_width"].isNull())
		stroke_width.SetJsonValue(root["stroke_width"]);
	if (!root["x_offset"].isNull())
		x_offset.SetJsonValue(root["x_offset"]);
	if (!root["y_offset"].isNull())
		y_offset.SetJsonValue(root["y_offset"]);
	if (!root["background_alpha"].isNull())
		background_alpha.SetJsonValue(root["background_alpha"]);
	if (!root["background_padding"].isNull())
		background_padding.SetJsonValue(root["background_padding"]);
	if (!root["background_corner"].isNull())
		background_corner.SetJsonValue(root["background_corner"]);
}

std::string Timer::PropertiesJSON(int64_t requested_frame) const
{
	Json::Value root = BasePropertiesJSON(requested_frame);

	root["mode"] = add_property_json("Mode", mode, "int", "", NULL, TIMER_MODE_COUNT_UP, TIMER_MODE_FRAME_NUMBER, false, requested_frame);
	root["mode"]["choices"].append(add_property_choice_json("Count Up", TIMER_MODE_COUNT_UP, mode));
	root["mode"]["choices"].append(add_property_choice_json("Count Down", TIMER_MODE_COUNT_DOWN, mode));
	root["mode"]["choices"].append(add_property_choice_json("Clock", TIMER_MODE_CLOCK, mode));
	root["mode"]["choices"].append(add_property_choice_json("Timecode", TIMER_MODE_TIMECODE, mode));
	root["mode"]["choices"].append(add_property_choice_json("Frame Number", TIMER_MODE_FRAME_NUMBER, mode));
	root["time_source"] = add_property_json("Time Source", time_source, "int", "", NULL, TIMER_TIME_CLIP, TIMER_TIME_SOURCE, false, requested_frame);
	root["time_source"]["choices"].append(add_property_choice_json("Clip Time", TIMER_TIME_CLIP, time_source));
	root["time_source"]["choices"].append(add_property_choice_json("Source Time", TIMER_TIME_SOURCE, time_source));
	root["format"] = add_property_json("Format", format, "int", "", NULL, TIMER_FORMAT_MM_SS, TIMER_FORMAT_FRAMES, false, requested_frame);
	root["format"]["choices"].append(add_property_choice_json("MM:SS", TIMER_FORMAT_MM_SS, format));
	root["format"]["choices"].append(add_property_choice_json("HH:MM:SS", TIMER_FORMAT_HH_MM_SS, format));
	root["format"]["choices"].append(add_property_choice_json("HH:MM:SS.mmm", TIMER_FORMAT_HH_MM_SS_MILLISECONDS, format));
	root["format"]["choices"].append(add_property_choice_json("Timecode", TIMER_FORMAT_TIMECODE, format));
	root["format"]["choices"].append(add_property_choice_json("Frames", TIMER_FORMAT_FRAMES, format));
	root["start_time"] = add_property_json("Start Time", start_time.GetValue(requested_frame), "float", "", &start_time, -86400.0, 86400.0, false, requested_frame);
	root["end_time"] = add_property_json("Countdown Duration", end_time.GetValue(requested_frame), "float", "", &end_time, 0.0, 86400.0, false, requested_frame);
	root["clamp"] = add_property_json("Clamp at Zero", clamp, "int", "", NULL, 0, 1, false, requested_frame);
	root["clamp"]["choices"].append(add_property_choice_json("Yes", 1, clamp));
	root["clamp"]["choices"].append(add_property_choice_json("No", 0, clamp));
	root["prefix"] = add_property_json("Prefix", 0.0, "string", prefix, NULL, -1, -1, false, requested_frame);
	root["suffix"] = add_property_json("Suffix", 0.0, "string", suffix, NULL, -1, -1, false, requested_frame);
	root["font_name"] = add_property_json("Font", 0.0, "font", font_name, NULL, -1, -1, false, requested_frame);
	root["font_size"] = add_property_json("Font Size", font_size.GetValue(requested_frame), "float", "", &font_size, 1.0, 300.0, false, requested_frame);
	root["font_alpha"] = add_property_json("Font Alpha", font_alpha.GetValue(requested_frame), "float", "", &font_alpha, 0.0, 1.0, false, requested_frame);
	root["color"] = add_property_json("Text Color", 0.0, "color", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["stroke"] = add_property_json("Stroke Color", 0.0, "color", "", &stroke.red, 0, 255, false, requested_frame);
	root["stroke"]["red"] = add_property_json("Red", stroke.red.GetValue(requested_frame), "float", "", &stroke.red, 0, 255, false, requested_frame);
	root["stroke"]["blue"] = add_property_json("Blue", stroke.blue.GetValue(requested_frame), "float", "", &stroke.blue, 0, 255, false, requested_frame);
	root["stroke"]["green"] = add_property_json("Green", stroke.green.GetValue(requested_frame), "float", "", &stroke.green, 0, 255, false, requested_frame);
	root["stroke_width"] = add_property_json("Stroke Width", stroke_width.GetValue(requested_frame), "float", "", &stroke_width, 0.0, 20.0, false, requested_frame);
	const int resolved_gravity = ClampGravity(gravity);
	root["gravity"] = add_property_json("Gravity", resolved_gravity, "int", "", NULL, GRAVITY_TOP_LEFT, GRAVITY_BOTTOM_RIGHT, false, requested_frame);
	root["gravity"]["choices"].append(add_property_choice_json("Top Left", GRAVITY_TOP_LEFT, resolved_gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Top Center", GRAVITY_TOP, resolved_gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Top Right", GRAVITY_TOP_RIGHT, resolved_gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Left", GRAVITY_LEFT, resolved_gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Center", GRAVITY_CENTER, resolved_gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Right", GRAVITY_RIGHT, resolved_gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Bottom Left", GRAVITY_BOTTOM_LEFT, resolved_gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Bottom Center", GRAVITY_BOTTOM, resolved_gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Bottom Right", GRAVITY_BOTTOM_RIGHT, resolved_gravity));
	root["x_offset"] = add_property_json("X Offset (%)", x_offset.GetValue(requested_frame), "float", "", &x_offset, -100.0, 100.0, false, requested_frame);
	root["y_offset"] = add_property_json("Y Offset (%)", y_offset.GetValue(requested_frame), "float", "", &y_offset, -100.0, 100.0, false, requested_frame);
	root["show_background"] = add_property_json("Show Background", show_background, "int", "", NULL, 0, 1, false, requested_frame);
	root["show_background"]["choices"].append(add_property_choice_json("Yes", 1, show_background));
	root["show_background"]["choices"].append(add_property_choice_json("No", 0, show_background));
	root["background"] = add_property_json("Background Color", 0.0, "color", "", &background.red, 0, 255, false, requested_frame);
	root["background"]["red"] = add_property_json("Red", background.red.GetValue(requested_frame), "float", "", &background.red, 0, 255, false, requested_frame);
	root["background"]["blue"] = add_property_json("Blue", background.blue.GetValue(requested_frame), "float", "", &background.blue, 0, 255, false, requested_frame);
	root["background"]["green"] = add_property_json("Green", background.green.GetValue(requested_frame), "float", "", &background.green, 0, 255, false, requested_frame);
	root["background_alpha"] = add_property_json("Background Alpha", background_alpha.GetValue(requested_frame), "float", "", &background_alpha, 0.0, 1.0, false, requested_frame);
	root["background_padding"] = add_property_json("Background Padding", background_padding.GetValue(requested_frame), "float", "", &background_padding, 0.0, 100.0, false, requested_frame);
	root["background_corner"] = add_property_json("Background Corner Radius", background_corner.GetValue(requested_frame), "float", "", &background_corner, 0.0, 100.0, false, requested_frame);

	return root.toStyledString();
}
