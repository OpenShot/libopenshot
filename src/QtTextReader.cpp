/**
 * @file
 * @brief Source file for QtTextReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Sergei Kolesov (jediserg)
 * @author Jeff Shillitto (jeffski)
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "QtTextReader.h"
#include "CacheBase.h"
#include "Clip.h"
#include "Timeline.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Color.h"

#include <QImage>
#include <QPainter>
#include <QTextDocument>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <algorithm>
#include <cmath>

using namespace openshot;

/// Default constructor (blank text)
QtTextReader::QtTextReader() : width(1024), height(768), x_offset(0), y_offset(0), text(""), font(QFont("Arial", 10)), text_color("#ffffff"), background_color("#000000"), is_open(false), gravity(GRAVITY_CENTER)
{
	// Populate attributes (such as height, width, etc...) without rendering
	UpdateInfo();
}

QtTextReader::QtTextReader(int width, int height, int x_offset, int y_offset, GravityType gravity, std::string text, QFont font, std::string text_color, std::string background_color)
: width(width), height(height), x_offset(x_offset), y_offset(y_offset), text(text), font(font), text_color(text_color), background_color(background_color), is_open(false), gravity(gravity)
{
	// Populate attributes (such as height, width, etc...) without rendering
	UpdateInfo();
}

void QtTextReader::SetTextBackgroundColor(std::string color) {
	text_background_color = color;

	// Re-render an open reader with the new text background color
	if (is_open) {
		Close();
		Open();
	}
}

// Clip-local style curves are evaluated in source frames, like other readers.
Json::Value QtTextReader::EvaluateStyle(int64_t frame) const {
    Json::Value result(Json::objectValue);
    for (const auto& key : text_animation.getMemberNames()) {
        if (key == "color" || key == "background" || key == "stroke" || key == "border") {
            Color color("#ffffff");
            color.SetJsonValue(text_animation[key]);
            auto rgba = color.GetColorRGBA(frame);
            result[key] = QColor(rgba[0], rgba[1], rgba[2], rgba[3]).name(QColor::HexArgb).toStdString();
        } else if (key == "stroke_width" || key == "border_width" || key == "background_corner") {
            Keyframe value;
            value.SetJsonValue(text_animation[key]);
            result[key] = std::max(0.0, value.GetValue(frame));
        }
    }
    return result;
}

// Raster scale that keeps this text sharp at the parent clip's largest size,
// like QtImageReader does for SVGs. SCALE_NONE draws a source at its pixel
// size, so it also follows the preview-to-project ratio, and `detail` is the
// extra scale that Clip shrinks back out (see DetailScale).
double QtTextReader::RasterScale(double& detail) {
    detail = 1.0;
    Clip* parent = (Clip*) ParentClip();
    if (!parent || !parent->ParentTimeline())
        return 1.0;
    Timeline* timeline = (Timeline*) parent->ParentTimeline();
    if (timeline->preview_width <= 0 || timeline->preview_height <= 0 || timeline->info.width <= 0)
        return 1.0;
    const double clip_scale = std::max({1.0, double(parent->scale_x.GetMaxPoint().co.Y),
                                        double(parent->scale_y.GetMaxPoint().co.Y)});
    // Bound the raster (a 16x scale of a full-frame box would need gigabytes)
    const double max_scale = std::max(1.0, 8192.0 / std::max(width, height));
    if (parent->scale == SCALE_NONE) {
        const double preview = timeline->preview_width / double(timeline->info.width);
        detail = std::max(1.0, std::min(clip_scale, max_scale / preview));
        return preview * detail;
    }
    // Other modes resize the image to the preview, so only its pixel count matters
    QSizeF size(width, height);
    size.scale(timeline->preview_width, timeline->preview_height,
               parent->scale == SCALE_CROP ? Qt::KeepAspectRatioByExpanding :
               parent->scale == SCALE_STRETCH ? Qt::IgnoreAspectRatio : Qt::KeepAspectRatio);
    const double fit = std::max(size.width() / width, size.height() / height);
    return std::min(fit * clip_scale, max_scale);
}

// Render one style into a new image (layout in project pixels, raster at render_scale)
std::shared_ptr<QImage> QtTextReader::Render(const Json::Value& style)
{
        const auto color_at = [&](const char* key, const std::string& fallback) {
            return style.isMember(key) ? style[key].asString() : fallback;
        };
        const auto number_at = [&](const char* key, double fallback) {
            return style.isMember(key) ? style[key].asDouble() : fallback;
        };
        const auto text_color = color_at("color", this->text_color);
        const auto background_color = color_at("background", this->background_color);
        const auto stroke = color_at("stroke", this->stroke);
        const auto border = color_at("border", this->border);
        const auto stroke_width = number_at("stroke_width", this->stroke_width);
        const auto border_width = number_at("border_width", this->border_width);
        // A radius past half the box would only distort the curve.
        const double corner = std::min(std::max(0.0, number_at("background_corner", this->background_corner)),
                                       std::min(width, height) / 2.0);

		// create image (layout stays in project pixels; only the raster is scaled)
		const int image_width = std::max(1, int(std::round(width * render_scale)));
		const int image_height = std::max(1, int(std::round(height * render_scale)));
		auto image = std::make_shared<QImage>(image_width, image_height, QImage::Format_RGBA8888_Premultiplied);
		image->fill(corner > 0 ? QColor(Qt::transparent) : QColor(background_color.c_str()));

		QPainter painter;
		if (!painter.begin(image.get())) {
			return nullptr;
		}
		painter.scale(image_width / double(width), image_height / double(height));
		if (corner > 0) {
			painter.save();
			painter.setRenderHint(QPainter::Antialiasing);
			painter.setPen(Qt::NoPen);
			painter.setBrush(QColor(background_color.c_str()));
			painter.drawRoundedRect(QRectF(0, 0, width, height), corner, corner);
			painter.restore();
		}

		// set background
		if (!text_background_color.empty()) {
			painter.setBackgroundMode(Qt::OpaqueMode);
			painter.setBackground(QBrush(text_background_color.c_str()));
		}

		// set font color
		painter.setPen(QPen(text_color.c_str()));

		// set font
		QFont text_font(font);
		if (letter_spacing != 0.0)
			text_font.setLetterSpacing(QFont::AbsoluteSpacing, letter_spacing);
		painter.setFont(text_font);

		// Set gravity (map between OpenShot and Qt)
		int align_flag = 0;
		switch (gravity)
		{
		case GRAVITY_TOP_LEFT:
			align_flag = Qt::AlignLeft | Qt::AlignTop;
			break;
		case GRAVITY_TOP:
			align_flag = Qt::AlignHCenter | Qt::AlignTop;
			break;
		case GRAVITY_TOP_RIGHT:
			align_flag = Qt::AlignRight | Qt::AlignTop;
			break;
		case GRAVITY_LEFT:
			align_flag = Qt::AlignVCenter | Qt::AlignLeft;
			break;
		case GRAVITY_CENTER:
			align_flag = Qt::AlignCenter;
			break;
		case GRAVITY_RIGHT:
			align_flag = Qt::AlignVCenter | Qt::AlignRight;
			break;
		case GRAVITY_BOTTOM_LEFT:
			align_flag = Qt::AlignLeft | Qt::AlignBottom;
			break;
		case GRAVITY_BOTTOM:
			align_flag = Qt::AlignHCenter | Qt::AlignBottom;
			break;
		case GRAVITY_BOTTOM_RIGHT:
			align_flag = Qt::AlignRight | Qt::AlignBottom;
			break;
		}

		// Draw image
		if (document_layout) {
            QTextDocument document;
            document.setDefaultFont(text_font);
            document.setDocumentMargin(background_padding);
            QTextOption option;
            option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            option.setAlignment(Qt::Alignment(align_flag & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter)));
            document.setDefaultTextOption(option);
            document.setPlainText(QString::fromStdString(text));
            document.setTextWidth(width);
            QTextCursor cursor(&document);
            cursor.select(QTextCursor::Document);
            if (line_spacing != 1.0) {
                QTextBlockFormat block;
                block.setLineHeight(line_spacing * 100.0, QTextBlockFormat::ProportionalHeight);
                cursor.mergeBlockFormat(block);
            }
            QAbstractTextDocumentLayout::PaintContext context;
            context.palette.setColor(QPalette::Text, QColor(QString::fromStdString(text_color)));
            painter.save();
            painter.translate(x_offset, y_offset);
            QTextCharFormat format;
            if (stroke_width > 0) {
                // Outline first, then the fill on top, so a wide outline
                // grows outward instead of covering the letters (as Caption).
                format.setForeground(Qt::transparent);
                format.setTextOutline(QPen(QColor(QString::fromStdString(stroke)), stroke_width));
                cursor.mergeCharFormat(format);
                document.documentLayout()->draw(&painter, context);
                format.setTextOutline(QPen(Qt::transparent, 0));  // Fill as a path, like the outline.
            }
            format.setForeground(QColor(QString::fromStdString(text_color)));
            cursor.mergeCharFormat(format);
            document.documentLayout()->draw(&painter, context);
            painter.restore();
            if (border_width > 0) {
                // Square corners at any width (the default bevel cuts them off).
                QPen border_pen(QColor(QString::fromStdString(border)), border_width);
                border_pen.setJoinStyle(Qt::MiterJoin);
                painter.setPen(border_pen);
                painter.setBrush(Qt::NoBrush);
                const QRectF border_rect(border_width / 2, border_width / 2,
                                         width - border_width, height - border_width);
                if (corner > 0) {
                    // The border's outer edge follows the background's curve.
                    const double radius = std::max(0.0, corner - border_width / 2);
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.drawRoundedRect(border_rect, radius, radius);
                } else {
                    painter.drawRect(border_rect);
                }
            }
        } else {
            painter.drawText(x_offset, y_offset, width, height, align_flag, text.c_str());
        }

		painter.end();

		return image;
}

// Populate reader info from the current properties
void QtTextReader::UpdateInfo()
{
		info.has_audio = false;
		info.has_video = true;
		info.has_single_image = text_animation.empty();
		info.file_size = 0;
		info.vcodec = "QImage";
		info.width = width;
		info.height = height;
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = 60 * 60 * 1;  // 1 hour duration
		// Style curves use the project's frame numbers, so match its frame rate
		// and let the timeline's FrameMapper pass frames through unchanged.
		info.fps = source_fps;
		info.video_timebase = source_fps.Reciprocal();
		info.video_length = round(info.duration * info.fps.ToDouble());

		// Calculate the DAR (display aspect ratio)
		Fraction font_size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

		// Reduce size fraction
		font_size.Reduce();

		// Set the ratio based on the reduced fraction
		info.display_ratio.num = font_size.num;
		info.display_ratio.den = font_size.den;
}

// Show a style, reusing a recent render of an identical style when possible
void QtTextReader::UseStyle(const Json::Value& style)
{
	const std::string key = style.toStyledString();
	for (auto entry = render_cache.begin(); entry != render_cache.end(); ++entry) {
		if (entry->first == key) {
			render_cache.splice(render_cache.begin(), render_cache, entry);
			image = entry->second;
			rendered_style = style;
			return;
		}
	}
	auto rendered = Render(style);
	if (!rendered)
		return;
	image = rendered;
	rendered_style = style;
	render_cache.emplace_front(key, rendered);
	// Out-of-order frames from parallel rendering must not re-render held
	// styles, but cached rasters stay small: a few entries, 16 MB at most.
	const qint64 max_bytes = 16 * 1024 * 1024;
	qint64 bytes = 0;
	size_t count = 0;
	for (auto entry = render_cache.begin(); entry != render_cache.end();) {
		bytes += entry->second->sizeInBytes();
		if (++count > 4 || (count > 1 && bytes > max_bytes))
			entry = render_cache.erase(entry);
		else
			++entry;
	}
}

// Open reader
void QtTextReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		UseStyle(EvaluateStyle(render_frame));
		if (!image)
			return;

		// Update image properties
		UpdateInfo();

		// Mark as "open"
		is_open = true;
	}
}

// Close reader
void QtTextReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Mark as "closed"
		is_open = false;

		// Delete the image and recent renders
		image.reset();
		render_cache.clear();

		info.vcodec = "";
		info.acodec = "";
	}
}

// Get an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> QtTextReader::GetFrame(int64_t requested_frame)
{
	// Create a scoped lock, allowing only a single thread to run the following code at one time
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

	auto sample_count = Frame::GetSamplesPerFrame(requested_frame, info.fps, info.sample_rate, info.channels);

    double detail = 1.0;
    const double raster_scale = RasterScale(detail);
    if (std::abs(raster_scale - render_scale) > 1e-6 || std::abs(detail - detail_scale) > 1e-6) {
        render_scale = raster_scale;
        detail_scale = detail;
        render_frame = requested_frame;
        Close();
        Open();
    }
    if (is_open && !text_animation.empty()) {
        const auto style = EvaluateStyle(requested_frame);
        if (style != rendered_style)
            UseStyle(style);
    }

	if (image)
	{
		// Create or get frame object
		auto image_frame = std::make_shared<Frame>(
			requested_frame, image->size().width(), image->size().height(),
			background_color, sample_count, info.channels);

		// Add Image data to frame
		image_frame->AddImage(image);

		// return frame object
		return image_frame;
	} else {
		// return empty frame
		auto image_frame = std::make_shared<Frame>(1, 640, 480, background_color, sample_count, info.channels);

		// return frame object
		return image_frame;
	}
}

// Generate JSON string of this object
std::string QtTextReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value QtTextReader::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "QtTextReader";
    root["document_layout"] = document_layout;
    root["text_animation"] = text_animation;
    root["background_padding"] = background_padding;
    root["stroke_width"] = stroke_width;
    root["stroke"] = stroke;
    root["border_width"] = border_width;
    root["border"] = border;
    root["letter_spacing"] = letter_spacing;
    root["line_spacing"] = line_spacing;
    root["background_corner"] = background_corner;
	root["width"] = width;
	root["height"] = height;
	root["x_offset"] = x_offset;
	root["y_offset"] = y_offset;
	root["text"] = text;
	root["font"] = font.toString().toStdString();
	root["text_color"] = text_color;
	root["background_color"] = background_color;
	root["text_background_color"] = text_background_color;
	root["gravity"] = gravity;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void QtTextReader::SetJson(const std::string value) {

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
void QtTextReader::SetJsonValue(const Json::Value root) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["width"].isNull())
		width = root["width"].asInt();
	if (!root["height"].isNull())
		height = root["height"].asInt();
	if (!root["x_offset"].isNull())
		x_offset = root["x_offset"].asInt();
	if (!root["y_offset"].isNull())
		y_offset = root["y_offset"].asInt();
	if (!root["text"].isNull())
		text = root["text"].asString();
	if (!root["font"].isNull())
		font.fromString(QString::fromStdString(root["font"].asString()));
	if (!root["text_color"].isNull())
		text_color = root["text_color"].asString();
	if (!root["background_color"].isNull())
		background_color = root["background_color"].asString();
	if (!root["text_background_color"].isNull())
		text_background_color = root["text_background_color"].asString();
	if (!root["gravity"].isNull())
		gravity = (GravityType) root["gravity"].asInt();

	if (root.isMember("text_animation")) text_animation = root["text_animation"];
    if (!root["document_layout"].isNull()) document_layout = root["document_layout"].asBool();
    if (!root["background_padding"].isNull()) background_padding = std::max(0.0, root["background_padding"].asDouble());
    if (!root["stroke_width"].isNull()) stroke_width = std::max(0.0, root["stroke_width"].asDouble());
    if (!root["stroke"].isNull()) stroke = root["stroke"].asString();
    if (!root["border_width"].isNull()) border_width = std::max(0.0, root["border_width"].asDouble());
    if (!root["border"].isNull()) border = root["border"].asString();
    if (!root["letter_spacing"].isNull()) letter_spacing = root["letter_spacing"].asDouble();
    if (!root["line_spacing"].isNull()) line_spacing = std::max(0.1, root["line_spacing"].asDouble());
    if (!root["background_corner"].isNull()) background_corner = std::max(0.0, root["background_corner"].asDouble());
    width = std::max(1, width);
    height = std::max(1, height);
    if (root["fps"].isObject() && root["fps"]["num"].asInt() > 0 && root["fps"]["den"].asInt() > 0)
        source_fps = Fraction(root["fps"]["num"].asInt(), root["fps"]["den"].asInt());
    render_cache.clear();
    UpdateInfo();

    // Re-Open path, and re-init everything (if needed)
	if (is_open)
	{
		Close();
		Open();
	}
}
