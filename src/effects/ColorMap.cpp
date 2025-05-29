/**
 * @file
 * @brief Source file for ColorMap (LUT) effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ColorMap.h"
#include "Exceptions.h"
#include <omp.h>
#include <QRegularExpression>

using namespace openshot;

void ColorMap::load_cube_file()
{
    if (lut_path.empty()) {
        lut_data.clear();
        lut_size = 0;
        needs_refresh = false;
        return;
    }

    int parsed_size = 0;
    std::vector<float> parsed_data;

    #pragma omp critical(load_lut)
    {
        QFile file(QString::fromStdString(lut_path));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // leave parsed_size == 0
        } else {
            QTextStream in(&file);
            QString line;
            QRegularExpression ws_re("\\s+");

            // 1) Find LUT_3D_SIZE
            while (!in.atEnd()) {
                line = in.readLine().trimmed();
                if (line.startsWith("LUT_3D_SIZE")) {
                    auto parts = line.split(ws_re);
                    if (parts.size() >= 2) {
                        parsed_size = parts[1].toInt();
                    }
                    break;
                }
            }

            // 2) Read N³ lines of R G B floats
            if (parsed_size > 0) {
                int total = parsed_size * parsed_size * parsed_size;
                parsed_data.reserve(size_t(total * 3));
                while (!in.atEnd() && int(parsed_data.size()) < total * 3) {
                    line = in.readLine().trimmed();
                    if (line.isEmpty() ||
                        line.startsWith("#") ||
                        line.startsWith("TITLE") ||
                        line.startsWith("DOMAIN"))
                    {
                        continue;
                    }
                    auto vals = line.split(ws_re);
                    if (vals.size() >= 3) {
                        // .cube file is R G B
                        parsed_data.push_back(vals[0].toFloat());
                        parsed_data.push_back(vals[1].toFloat());
                        parsed_data.push_back(vals[2].toFloat());
                    }
                }
                if (int(parsed_data.size()) != total * 3) {
                    parsed_data.clear();
                    parsed_size = 0;
                }
            }
        }
    }

    if (parsed_size > 0) {
        lut_size = parsed_size;
        lut_data.swap(parsed_data);
    } else {
        lut_data.clear();
        lut_size = 0;
    }
    needs_refresh = false;
}

void ColorMap::init_effect_details()
{
    InitEffectInfo();
    info.class_name = "ColorMap";
    info.name       = "Color Map / Lookup";
    info.description = "Adjust colors using 3D LUT lookup tables (.cube format)";
    info.has_video  = true;
    info.has_audio  = false;
}

ColorMap::ColorMap()
    : lut_path(""), lut_size(0), needs_refresh(true),
      intensity(1.0), intensity_r(1.0), intensity_g(1.0), intensity_b(1.0)
{
    init_effect_details();
    load_cube_file();
}

ColorMap::ColorMap(const std::string &path,
                   const Keyframe &i,
                   const Keyframe &iR,
                   const Keyframe &iG,
                   const Keyframe &iB)
    : lut_path(path),
      lut_size(0),
      needs_refresh(true),
      intensity(i),
      intensity_r(iR),
      intensity_g(iG),
      intensity_b(iB)
{
    init_effect_details();
    load_cube_file();
}

std::shared_ptr<openshot::Frame>
ColorMap::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
    // Reload LUT when its path changed; no locking here
    if (needs_refresh) {
        load_cube_file();
        needs_refresh = false;
    }

    if (lut_data.empty())
        return frame;

    auto image = frame->GetImage();
    int w = image->width(), h = image->height();
    unsigned char *pixels = image->bits();

    float overall = float(intensity.GetValue(frame_number));
    float tR = float(intensity_r.GetValue(frame_number)) * overall;
    float tG = float(intensity_g.GetValue(frame_number)) * overall;
    float tB = float(intensity_b.GetValue(frame_number)) * overall;

    int pixel_count = w * h;
    #pragma omp parallel for
    for (int i = 0; i < pixel_count; ++i) {
        int idx = i * 4;
        int A = pixels[idx + 3];
        float alpha = A / 255.0f;
        if (alpha == 0.0f) continue;

        // demultiply premultiplied RGBA
        float R = pixels[idx + 0] / alpha;
        float G = pixels[idx + 1] / alpha;
        float B = pixels[idx + 2] / alpha;

        // normalize to [0,1]
        float Rn = R * (1.0f / 255.0f);
        float Gn = G * (1.0f / 255.0f);
        float Bn = B * (1.0f / 255.0f);

        // map into LUT space [0 .. size-1]
        float rf = Rn * (lut_size - 1);
        float gf = Gn * (lut_size - 1);
        float bf = Bn * (lut_size - 1);

        int r0 = int(floor(rf)), r1 = std::min(r0 + 1, lut_size - 1);
        int g0 = int(floor(gf)), g1 = std::min(g0 + 1, lut_size - 1);
        int b0 = int(floor(bf)), b1 = std::min(b0 + 1, lut_size - 1);

        float dr = rf - r0;
        float dg = gf - g0;
        float db = bf - b0;

        // compute base offsets with red fastest, then green, then blue
        int base000 = ((b0 * lut_size + g0) * lut_size + r0) * 3;
        int base100 = ((b0 * lut_size + g0) * lut_size + r1) * 3;
        int base010 = ((b0 * lut_size + g1) * lut_size + r0) * 3;
        int base110 = ((b0 * lut_size + g1) * lut_size + r1) * 3;
        int base001 = ((b1 * lut_size + g0) * lut_size + r0) * 3;
        int base101 = ((b1 * lut_size + g0) * lut_size + r1) * 3;
        int base011 = ((b1 * lut_size + g1) * lut_size + r0) * 3;
        int base111 = ((b1 * lut_size + g1) * lut_size + r1) * 3;

        // trilinear interpolation
        // red
        float c00 = lut_data[base000 + 0] * (1 - dr) + lut_data[base100 + 0] * dr;
        float c01 = lut_data[base001 + 0] * (1 - dr) + lut_data[base101 + 0] * dr;
        float c10 = lut_data[base010 + 0] * (1 - dr) + lut_data[base110 + 0] * dr;
        float c11 = lut_data[base011 + 0] * (1 - dr) + lut_data[base111 + 0] * dr;
        float c0  = c00 * (1 - dg) + c10 * dg;
        float c1  = c01 * (1 - dg) + c11 * dg;
        float lr = c0 * (1 - db) + c1 * db;

        // green
        c00 = lut_data[base000 + 1] * (1 - dr) + lut_data[base100 + 1] * dr;
        c01 = lut_data[base001 + 1] * (1 - dr) + lut_data[base101 + 1] * dr;
        c10 = lut_data[base010 + 1] * (1 - dr) + lut_data[base110 + 1] * dr;
        c11 = lut_data[base011 + 1] * (1 - dr) + lut_data[base111 + 1] * dr;
        c0  = c00 * (1 - dg) + c10 * dg;
        c1  = c01 * (1 - dg) + c11 * dg;
        float lg = c0 * (1 - db) + c1 * db;

        // blue
        c00 = lut_data[base000 + 2] * (1 - dr) + lut_data[base100 + 2] * dr;
        c01 = lut_data[base001 + 2] * (1 - dr) + lut_data[base101 + 2] * dr;
        c10 = lut_data[base010 + 2] * (1 - dr) + lut_data[base110 + 2] * dr;
        c11 = lut_data[base011 + 2] * (1 - dr) + lut_data[base111 + 2] * dr;
        c0  = c00 * (1 - dg) + c10 * dg;
        c1  = c01 * (1 - dg) + c11 * dg;
        float lb = c0 * (1 - db) + c1 * db;

        // blend per-channel, re-premultiply alpha
        float outR = (lr * tR + Rn * (1 - tR)) * alpha;
        float outG = (lg * tG + Gn * (1 - tG)) * alpha;
        float outB = (lb * tB + Bn * (1 - tB)) * alpha;

        pixels[idx + 0] = constrain(outR * 255.0f);
        pixels[idx + 1] = constrain(outG * 255.0f);
        pixels[idx + 2] = constrain(outB * 255.0f);
        // alpha left unchanged
    }

    return frame;
}


std::string ColorMap::Json() const
{
    return JsonValue().toStyledString();
}

Json::Value ColorMap::JsonValue() const
{
    Json::Value root = EffectBase::JsonValue();
    root["type"]         = info.class_name;
    root["lut_path"]     = lut_path;
    root["intensity"] = intensity.JsonValue();
    root["intensity_r"] = intensity_r.JsonValue();
    root["intensity_g"] = intensity_g.JsonValue();
    root["intensity_b"] = intensity_b.JsonValue();
    return root;
}

void ColorMap::SetJson(const std::string value)
{
    try {
        const Json::Value root = openshot::stringToJson(value);
        SetJsonValue(root);
    }
    catch (...) {
        throw InvalidJSON("Invalid JSON for ColorMap effect");
    }
}

void ColorMap::SetJsonValue(const Json::Value root)
{
    EffectBase::SetJsonValue(root);
    if (!root["lut_path"].isNull())
    {
        lut_path = root["lut_path"].asString();
        needs_refresh = true;
    }
    if (!root["intensity"].isNull())
        intensity.SetJsonValue(root["intensity"]);
    if (!root["intensity_r"].isNull())
        intensity_r.SetJsonValue(root["intensity_r"]);
    if (!root["intensity_g"].isNull())
        intensity_g.SetJsonValue(root["intensity_g"]);
    if (!root["intensity_b"].isNull())
        intensity_b.SetJsonValue(root["intensity_b"]);
}

std::string ColorMap::PropertiesJSON(int64_t requested_frame) const
{
    Json::Value root = BasePropertiesJSON(requested_frame);

    root["lut_path"] = add_property_json(
        "LUT File", 0.0, "string", lut_path, nullptr, 0, 0, false, requested_frame);

    root["intensity"] = add_property_json(
        "Overall Intensity",
        intensity.GetValue(requested_frame),
        "float", "", &intensity, 0.0, 1.0, false, requested_frame);

    root["intensity_r"] = add_property_json(
        "Red Intensity",
        intensity_r.GetValue(requested_frame),
        "float", "", &intensity_r, 0.0, 1.0, false, requested_frame);

    root["intensity_g"] = add_property_json(
        "Green Intensity",
        intensity_g.GetValue(requested_frame),
        "float", "", &intensity_g, 0.0, 1.0, false, requested_frame);

    root["intensity_b"] = add_property_json(
        "Blue Intensity",
        intensity_b.GetValue(requested_frame),
        "float", "", &intensity_b, 0.0, 1.0, false, requested_frame);

    return root.toStyledString();
}
