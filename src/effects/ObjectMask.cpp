/**
 * @file
 * @brief Source file for Object Mask effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "effects/ObjectMask.h"

#include "Exceptions.h"
#include "Frame.h"
#include "objdetectdata.pb.h"

#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#undef uint64
#undef int64

#include <QColor>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <fstream>

using namespace openshot;

namespace {

QImage AlphaMaskImageFromRLE(const ObjectMaskFrameData& mask)
{
    QImage image(mask.width, mask.height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!mask.HasData())
        return image;

    QRgb* data = reinterpret_cast<QRgb*>(image.bits());
    const int total = mask.width * mask.height;
    int offset = 0;
    bool value = false;
    for (uint32_t count : mask.rle) {
        const int end = std::min(total, offset + static_cast<int>(count));
        if (value)
            std::fill(data + offset, data + end, qRgba(255, 255, 255, 255));
        offset = end;
        value = !value;
        if (offset >= total)
            break;
    }
    return image;
}

cv::Mat BinaryMaskFromImage(const QImage& image)
{
    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    cv::Mat binary(rgba.height(), rgba.width(), CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < rgba.height(); ++y) {
        const uchar* source = rgba.constScanLine(y);
        uchar* target = binary.ptr<uchar>(y);
        for (int x = 0; x < rgba.width(); ++x)
            target[x] = source[x * 4 + 3] > 0 ? 255 : 0;
    }
    return binary;
}

QImage StrokeImageFromMask(const QImage& alphaMask, int width)
{
    QImage result(alphaMask.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    if (width <= 0)
        return result;

    cv::Mat binary = BinaryMaskFromImage(alphaMask);
    cv::Mat dilated;
    const int kernelSize = std::max(1, width * 2 + 1);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(kernelSize, kernelSize));
    cv::dilate(binary, dilated, kernel);
    cv::Mat edge = dilated - binary;

    for (int y = 0; y < edge.rows; ++y) {
        const uchar* edgeRow = edge.ptr<uchar>(y);
        QRgb* target = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < edge.cols; ++x) {
            if (edgeRow[x])
                target[x] = qRgba(255, 255, 255, 255);
        }
    }
    return result;
}

}

ObjectMask::ObjectMask()
    : draw_mask(1.0)
    , mask_color(83, 160, 237, 255)
    , mask_alpha(120.0 / 255.0)
    , stroke_color(255, 255, 255, 255)
    , stroke_alpha(1.0)
    , stroke_width(3.0)
{
    init_effect_details();
}

void ObjectMask::init_effect_details()
{
    InitEffectInfo();
    info.class_name = "ObjectMask";
    info.name = "Object Mask";
    info.description = "Create and draw a segmentation mask for a prompted object.";
    info.has_audio = false;
    info.has_video = true;
    info.has_tracked_object = true;
}

std::shared_ptr<Frame> ObjectMask::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
    std::shared_ptr<QImage> frame_image = frame->GetImage();
    if (!frame_image || frame_image->isNull() || draw_mask.GetValue(frame_number) != 1)
        return frame;

    auto mask_it = masksData.find(frame_number);
    if (mask_it == masksData.end() || !mask_it->second.HasData())
        return frame;

    QImage alpha_mask = AlphaMaskImageFromRLE(mask_it->second)
        .scaled(frame_image->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    std::vector<int> mask_rgba = mask_color.GetColorRGBA(frame_number);
    QColor overlay_color(mask_rgba[0], mask_rgba[1], mask_rgba[2], 255 * mask_alpha.GetValue(frame_number));

    QImage overlay(frame_image->size(), QImage::Format_ARGB32_Premultiplied);
    overlay.fill(Qt::transparent);
    QPainter overlay_painter(&overlay);
    overlay_painter.setCompositionMode(QPainter::CompositionMode_Source);
    overlay_painter.fillRect(overlay.rect(), overlay_color);
    overlay_painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    overlay_painter.drawImage(0, 0, alpha_mask);
    overlay_painter.end();

    QPainter painter(frame_image.get());
    painter.drawImage(0, 0, overlay);

    const int strokeWidth = static_cast<int>(std::round(stroke_width.GetValue(frame_number)));
    if (strokeWidth > 0 && stroke_alpha.GetValue(frame_number) > 0.0) {
        QImage stroke_mask = StrokeImageFromMask(alpha_mask, strokeWidth);
        std::vector<int> stroke_rgba = stroke_color.GetColorRGBA(frame_number);
        QColor stroke_qcolor(stroke_rgba[0], stroke_rgba[1], stroke_rgba[2], 255 * stroke_alpha.GetValue(frame_number));

        QImage stroke_overlay(frame_image->size(), QImage::Format_ARGB32_Premultiplied);
        stroke_overlay.fill(Qt::transparent);
        QPainter stroke_painter(&stroke_overlay);
        stroke_painter.setCompositionMode(QPainter::CompositionMode_Source);
        stroke_painter.fillRect(stroke_overlay.rect(), stroke_qcolor);
        stroke_painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        stroke_painter.drawImage(0, 0, stroke_mask);
        stroke_painter.end();
        painter.drawImage(0, 0, stroke_overlay);
    }
    painter.end();

    return frame;
}

bool ObjectMask::LoadObjMaskData(std::string inputFilePath)
{
    pb_objdetect::ObjDetect objMessage;
    std::fstream input(inputFilePath, std::ios::in | std::ios::binary);
    if (!objMessage.ParseFromIstream(&input))
        return false;

    masksData.clear();
    trackedObjects.clear();

    auto trackedObject = std::make_shared<TrackedObjectBBox>(83, 160, 237, 255);
    trackedObject->Id(Id().empty() ? "Object Mask" : Id() + "-1");
    trackedObject->ParentClip(this->ParentClip());
    trackedObject->draw_box = Keyframe(0.0);
    trackedObject->draw_text = Keyframe(0.0);
    trackedObject->draw_mask = draw_mask;
    trackedObject->mask_alpha = mask_alpha;
    trackedObject->mask_color = mask_color;
    trackedObject->stroke = stroke_color;
    trackedObject->stroke_alpha = stroke_alpha;
    trackedObject->stroke_width = stroke_width;

    for (int frameIndex = 0; frameIndex < objMessage.frame_size(); ++frameIndex) {
        const auto& pbFrame = objMessage.frame(frameIndex);
        if (pbFrame.bounding_box_size() <= 0)
            continue;

        const auto& box = pbFrame.bounding_box(0);
        ObjectMaskFrameData mask;
        mask.box = BBox(box.x() + box.w() / 2.0f, box.y() + box.h() / 2.0f, box.w(), box.h(), 0.0f);
        mask.score = box.confidence();
        if (box.has_mask()) {
            mask.width = box.mask().width();
            mask.height = box.mask().height();
            for (int rleIndex = 0; rleIndex < box.mask().rle_size(); ++rleIndex)
                mask.rle.push_back(box.mask().rle(rleIndex));
        }
        masksData[pbFrame.id()] = mask;

        trackedObject->AddBox(pbFrame.id(), mask.box.cx, mask.box.cy, mask.box.width, mask.box.height, 0.0f);
        if (mask.HasData()) {
            ObjectMaskData trackedMask;
            trackedMask.width = mask.width;
            trackedMask.height = mask.height;
            trackedMask.rle = mask.rle;
            trackedObject->AddMask(pbFrame.id(), trackedMask);
        }
    }

    if (!masksData.empty())
        trackedObjects[1] = trackedObject;

    google::protobuf::ShutdownProtobufLibrary();
    return true;
}

std::shared_ptr<QImage> ObjectMask::TrackedObjectMask(std::shared_ptr<QImage> target_image, int64_t frame_number) const
{
    if (!target_image || target_image->isNull())
        return {};

    auto mask_it = masksData.find(frame_number);
    if (mask_it == masksData.end() || !mask_it->second.HasData())
        return {};

    QImage alpha_mask = AlphaMaskImageFromRLE(mask_it->second)
        .scaled(target_image->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    auto mask_image = std::make_shared<QImage>(
        target_image->width(), target_image->height(), QImage::Format_RGBA8888_Premultiplied);
    mask_image->fill(QColor(0, 0, 0, 255));
    QPainter painter(mask_image.get());
    painter.drawImage(0, 0, alpha_mask);
    painter.end();
    return mask_image;
}

std::string ObjectMask::Json() const
{
    return JsonValue().toStyledString();
}

Json::Value ObjectMask::JsonValue() const
{
    Json::Value root = EffectBase::JsonValue();
    root["type"] = info.class_name;
    root["protobuf_data_path"] = protobuf_data_path;
    root["draw_mask"] = draw_mask.JsonValue();
    root["mask_color"] = mask_color.JsonValue();
    root["mask_alpha"] = mask_alpha.JsonValue();
    root["stroke_color"] = stroke_color.JsonValue();
    root["stroke_alpha"] = stroke_alpha.JsonValue();
    root["stroke_width"] = stroke_width.JsonValue();
    return root;
}

void ObjectMask::SetJson(const std::string value)
{
    try {
        SetJsonValue(openshot::stringToJson(value));
    } catch (const std::exception&) {
        throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
    }
}

void ObjectMask::SetJsonValue(const Json::Value root)
{
    EffectBase::SetJsonValue(root);

    if (!root["draw_mask"].isNull())
        draw_mask.SetJsonValue(root["draw_mask"]);
    if (!root["mask_color"].isNull())
        mask_color.SetJsonValue(root["mask_color"]);
    if (!root["mask_alpha"].isNull())
        mask_alpha.SetJsonValue(root["mask_alpha"]);
    if (!root["stroke_color"].isNull())
        stroke_color.SetJsonValue(root["stroke_color"]);
    if (!root["stroke"].isNull())
        stroke_color.SetJsonValue(root["stroke"]);
    if (!root["stroke_alpha"].isNull())
        stroke_alpha.SetJsonValue(root["stroke_alpha"]);
    if (!root["stroke_width"].isNull())
        stroke_width.SetJsonValue(root["stroke_width"]);

    if (!root["protobuf_data_path"].isNull()) {
        std::string new_path = root["protobuf_data_path"].asString();
        if (protobuf_data_path != new_path || masksData.empty()) {
            protobuf_data_path = new_path;
            if (!LoadObjMaskData(protobuf_data_path))
                throw InvalidFile("Invalid object mask protobuf data path", "");
        }
    }
}

std::string ObjectMask::PropertiesJSON(int64_t requested_frame) const
{
    Json::Value root = BasePropertiesJSON(requested_frame);
    root["protobuf_data_path"] = add_property_json("Object Mask Data", 0.0, "string", protobuf_data_path, NULL, -1, -1, false, requested_frame);

    root["draw_mask"] = add_property_json("Draw Mask", draw_mask.GetValue(requested_frame), "int", "", &draw_mask, 0, 1, false, requested_frame);
    root["draw_mask"]["choices"].append(add_property_choice_json("Yes", true, draw_mask.GetValue(requested_frame)));
    root["draw_mask"]["choices"].append(add_property_choice_json("No", false, draw_mask.GetValue(requested_frame)));

    root["mask_color"] = add_property_json("Mask Color", 0.0, "color", "", NULL, 0, 255, false, requested_frame);
    root["mask_color"]["red"] = add_property_json("Red", mask_color.red.GetValue(requested_frame), "float", "", &mask_color.red, 0, 255, false, requested_frame);
    root["mask_color"]["blue"] = add_property_json("Blue", mask_color.blue.GetValue(requested_frame), "float", "", &mask_color.blue, 0, 255, false, requested_frame);
    root["mask_color"]["green"] = add_property_json("Green", mask_color.green.GetValue(requested_frame), "float", "", &mask_color.green, 0, 255, false, requested_frame);
    root["mask_alpha"] = add_property_json("Mask Alpha", mask_alpha.GetValue(requested_frame), "float", "", &mask_alpha, 0.0, 1.0, false, requested_frame);

    root["stroke_color"] = add_property_json("Stroke Color", 0.0, "color", "", NULL, 0, 255, false, requested_frame);
    root["stroke_color"]["red"] = add_property_json("Red", stroke_color.red.GetValue(requested_frame), "float", "", &stroke_color.red, 0, 255, false, requested_frame);
    root["stroke_color"]["blue"] = add_property_json("Blue", stroke_color.blue.GetValue(requested_frame), "float", "", &stroke_color.blue, 0, 255, false, requested_frame);
    root["stroke_color"]["green"] = add_property_json("Green", stroke_color.green.GetValue(requested_frame), "float", "", &stroke_color.green, 0, 255, false, requested_frame);
    root["stroke_alpha"] = add_property_json("Stroke Alpha", stroke_alpha.GetValue(requested_frame), "float", "", &stroke_alpha, 0.0, 1.0, false, requested_frame);
    root["stroke_width"] = add_property_json("Stroke Width", stroke_width.GetValue(requested_frame), "int", "", &stroke_width, 0, 50, false, requested_frame);

    return root.toStyledString();
}
