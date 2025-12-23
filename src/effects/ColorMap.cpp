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
#include <algorithm>
#include <omp.h>
#include <QRegularExpression>

using namespace openshot;

void ColorMap::load_cube_file()
{
    if (lut_path.empty()) {
        lut_data.clear();
        lut_size = 0;
        lut_type = LUTType::None;
        needs_refresh = false;
        return;
    }

    int parsed_size = 0;
    std::vector<float> parsed_data;
    bool parsed_is_3d = false;
    std::array<float, 3> parsed_domain_min{0.0f, 0.0f, 0.0f};
    std::array<float, 3> parsed_domain_max{1.0f, 1.0f, 1.0f};

    #pragma omp critical(load_lut)
    {
        QFile file(QString::fromStdString(lut_path));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // leave parsed_size == 0
        } else {
            QTextStream in(&file);
            QRegularExpression ws_re("\\s+");
            auto parse_domain_line = [&](const QString &line) {
                if (!line.startsWith("DOMAIN_MIN") && !line.startsWith("DOMAIN_MAX"))
                    return;
                auto parts = line.split(ws_re);
                if (parts.size() < 4)
                    return;
                auto assign_values = [&](std::array<float, 3> &target) {
                    target[0] = parts[1].toFloat();
                    target[1] = parts[2].toFloat();
                    target[2] = parts[3].toFloat();
                };
                if (line.startsWith("DOMAIN_MIN"))
                    assign_values(parsed_domain_min);
                else
                    assign_values(parsed_domain_max);
            };

            auto try_parse = [&](const QString &keyword, bool want3d) -> bool {
                if (!file.seek(0) || !in.seek(0))
                    return false;

                QString line;
                int detected_size = 0;
                while (!in.atEnd()) {
                    line = in.readLine().trimmed();
                    parse_domain_line(line);
                    if (line.startsWith(keyword)) {
                        auto parts = line.split(ws_re);
                        if (parts.size() >= 2) {
                            detected_size = parts[1].toInt();
                        }
                        break;
                    }
                }
                if (detected_size <= 0)
                    return false;

                const int total_entries = want3d
                    ? detected_size * detected_size * detected_size
                    : detected_size;
                std::vector<float> data;
                data.reserve(size_t(total_entries * 3));
                while (!in.atEnd() && int(data.size()) < total_entries * 3) {
                    line = in.readLine().trimmed();
                    if (line.isEmpty() ||
                        line.startsWith("#") ||
                        line.startsWith("TITLE"))
                    {
                        continue;
                    }
                    if (line.startsWith("DOMAIN_MIN") ||
                        line.startsWith("DOMAIN_MAX"))
                    {
                        parse_domain_line(line);
                        continue;
                    }
                    auto vals = line.split(ws_re);
                    if (vals.size() >= 3) {
                        data.push_back(vals[0].toFloat());
                        data.push_back(vals[1].toFloat());
                        data.push_back(vals[2].toFloat());
                    }
                }
                if (int(data.size()) != total_entries * 3)
                    return false;

                parsed_size = detected_size;
                parsed_is_3d = want3d;
                parsed_data.swap(data);
                return true;
            };

            if (!try_parse("LUT_3D_SIZE", true)) {
                try_parse("LUT_1D_SIZE", false);
            }
        }
    }

    if (parsed_size > 0) {
        lut_size = parsed_size;
        lut_data.swap(parsed_data);
        lut_type = parsed_is_3d ? LUTType::LUT3D : LUTType::LUT1D;
        lut_domain_min = parsed_domain_min;
        lut_domain_max = parsed_domain_max;
    } else {
        lut_data.clear();
        lut_size = 0;
        lut_type = LUTType::None;
        lut_domain_min = std::array<float, 3>{0.0f, 0.0f, 0.0f};
        lut_domain_max = std::array<float, 3>{1.0f, 1.0f, 1.0f};
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
    : lut_path(""), lut_size(0), lut_type(LUTType::None), needs_refresh(true),
      lut_domain_min{0.0f, 0.0f, 0.0f}, lut_domain_max{1.0f, 1.0f, 1.0f},
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
      lut_type(LUTType::None),
      needs_refresh(true),
      lut_domain_min{0.0f, 0.0f, 0.0f}, lut_domain_max{1.0f, 1.0f, 1.0f},
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

    if (lut_data.empty() || lut_size <= 0 || lut_type == LUTType::None)
        return frame;

    auto image = frame->GetImage();
    int w = image->width(), h = image->height();
    unsigned char *pixels = image->bits();

    float overall = float(intensity.GetValue(frame_number));
    float tR = float(intensity_r.GetValue(frame_number)) * overall;
    float tG = float(intensity_g.GetValue(frame_number)) * overall;
    float tB = float(intensity_b.GetValue(frame_number)) * overall;

    const bool use3d = (lut_type == LUTType::LUT3D);
    const bool use1d = (lut_type == LUTType::LUT1D);
    const int lut_dim = lut_size;
    const std::vector<float> &table = lut_data;
    const int data_count = int(table.size());

    auto sample1d = [&](float value, int channel) -> float {
        if (lut_dim <= 1) {
            int base = std::min(channel, data_count - 1);
            return table[base];
        }
        float scaled = value * float(lut_dim - 1);
        int i0 = int(floor(scaled));
        int i1 = std::min(i0 + 1, lut_dim - 1);
        float t = scaled - i0;
        int base0 = std::max(0, std::min(i0 * 3 + channel, data_count - 1));
        int base1 = std::max(0, std::min(i1 * 3 + channel, data_count - 1));
        float v0 = table[base0];
        float v1 = table[base1];
        return v0 * (1.0f - t) + v1 * t;
    };

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

        auto normalize_to_domain = [&](float value, int channel) -> float {
            float min_val = lut_domain_min[channel];
            float max_val = lut_domain_max[channel];
            float range = max_val - min_val;
            if (range <= 0.0f)
                return std::clamp(value, 0.0f, 1.0f);
            float normalized = (value - min_val) / range;
            return std::clamp(normalized, 0.0f, 1.0f);
        };
        float Rdn = normalize_to_domain(Rn, 0);
        float Gdn = normalize_to_domain(Gn, 1);
        float Bdn = normalize_to_domain(Bn, 2);

        float lr = Rn;
        float lg = Gn;
        float lb = Bn;

        if (use3d) {
            float rf = Rdn * (lut_dim - 1);
            float gf = Gdn * (lut_dim - 1);
            float bf = Bdn * (lut_dim - 1);

            int r0 = int(floor(rf)), r1 = std::min(r0 + 1, lut_dim - 1);
            int g0 = int(floor(gf)), g1 = std::min(g0 + 1, lut_dim - 1);
            int b0 = int(floor(bf)), b1 = std::min(b0 + 1, lut_dim - 1);

            float dr = rf - r0;
            float dg = gf - g0;
            float db = bf - b0;

            int base000 = ((b0 * lut_dim + g0) * lut_dim + r0) * 3;
            int base100 = ((b0 * lut_dim + g0) * lut_dim + r1) * 3;
            int base010 = ((b0 * lut_dim + g1) * lut_dim + r0) * 3;
            int base110 = ((b0 * lut_dim + g1) * lut_dim + r1) * 3;
            int base001 = ((b1 * lut_dim + g0) * lut_dim + r0) * 3;
            int base101 = ((b1 * lut_dim + g0) * lut_dim + r1) * 3;
            int base011 = ((b1 * lut_dim + g1) * lut_dim + r0) * 3;
            int base111 = ((b1 * lut_dim + g1) * lut_dim + r1) * 3;

            float c00 = table[base000 + 0] * (1 - dr) + table[base100 + 0] * dr;
            float c01 = table[base001 + 0] * (1 - dr) + table[base101 + 0] * dr;
            float c10 = table[base010 + 0] * (1 - dr) + table[base110 + 0] * dr;
            float c11 = table[base011 + 0] * (1 - dr) + table[base111 + 0] * dr;
            float c0  = c00 * (1 - dg) + c10 * dg;
            float c1  = c01 * (1 - dg) + c11 * dg;
            lr = c0 * (1 - db) + c1 * db;

            c00 = table[base000 + 1] * (1 - dr) + table[base100 + 1] * dr;
            c01 = table[base001 + 1] * (1 - dr) + table[base101 + 1] * dr;
            c10 = table[base010 + 1] * (1 - dr) + table[base110 + 1] * dr;
            c11 = table[base011 + 1] * (1 - dr) + table[base111 + 1] * dr;
            c0  = c00 * (1 - dg) + c10 * dg;
            c1  = c01 * (1 - dg) + c11 * dg;
            lg = c0 * (1 - db) + c1 * db;

            c00 = table[base000 + 2] * (1 - dr) + table[base100 + 2] * dr;
            c01 = table[base001 + 2] * (1 - dr) + table[base101 + 2] * dr;
            c10 = table[base010 + 2] * (1 - dr) + table[base110 + 2] * dr;
            c11 = table[base011 + 2] * (1 - dr) + table[base111 + 2] * dr;
            c0  = c00 * (1 - dg) + c10 * dg;
            c1  = c01 * (1 - dg) + c11 * dg;
            lb = c0 * (1 - db) + c1 * db;
        } else if (use1d) {
            lr = sample1d(Rdn, 0);
            lg = sample1d(Gdn, 1);
            lb = sample1d(Bdn, 2);
        }

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
