/*
* Based on the FlareFX plug-in for GIMP 0.99 (version 1.05)
 * Original Copyright (C) 1997-1998 Karl-Johan Andersson <t96kja@student.tdb.uu.se>
 * Modifications May 2000 by Tim Copperfield <timecop@japan.co.jp>
 *
 * This code is available under the GNU GPL v2 (or any later version):
 *   You may redistribute and/or modify it under the terms of
 *   the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License,
 *   or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this code; if not, write to the Free Software Foundation,
 *   Inc., 59 Temple Place – Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * @file
 * @brief Header file for LensFlare class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "LensFlare.h"
#include "Exceptions.h"
#include <QImage>
#include <QPainter>
#include <QColor>
#include <cmath>
#include <vector>
#include <algorithm>
#include <omp.h>

using namespace openshot;

// Default constructor
LensFlare::LensFlare()
    : x(-0.5), y(-0.5), brightness(1.0), size(1.0), spread(1.0),
      color(Color("#ffffff"))
{
    init_effect_details();
}

// Parameterized constructor
LensFlare::LensFlare(const Keyframe &xPos,
                     const Keyframe &yPos,
                     const Keyframe &intensity,
                     const Keyframe &scale,
                     const Keyframe &spreadVal,
                     const Keyframe &bladeCount,
                     const Keyframe &shapeType,
                     const Color &tint)
    : x(xPos), y(yPos), brightness(intensity), size(scale),
      spread(spreadVal), color(tint)
{
    init_effect_details();
}

// Destructor
LensFlare::~LensFlare() = default;

// Initialize effect metadata
void LensFlare::init_effect_details()
{
    InitEffectInfo();
    info.class_name = "LensFlare";
    info.name = "Lens Flare";
    info.description = "Simulate sunlight hitting a lens with flares and spectral colors.";
    info.has_video = true;
    info.has_audio = false;
}

// Reflector definition
struct Reflect {
    float xp, yp, size;
    QColor col;
    int type; // 1..4
};

// Blend a color onto a pixel using additive blending
static inline QRgb blendAdd(QRgb dst, const QColor &c, float p)
{
    int dr = (255 - qRed(dst)) * p * c.redF();
    int dg = (255 - qGreen(dst)) * p * c.greenF();
    int db = (255 - qBlue(dst)) * p * c.blueF();
    int da = (255 - qAlpha(dst)) * p * c.alphaF();
    return qRgba(
        std::clamp(qRed(dst) + dr, 0, 255),
        std::clamp(qGreen(dst) + dg, 0, 255),
        std::clamp(qBlue(dst) + db, 0, 255),
        std::clamp(qAlpha(dst) + da, 0, 255)
    );
}

// Shift HSV values by given factors
static QColor shifted_hsv(const QColor &base, float h_shift,
                          float s_scale, float v_scale,
                          float a_scale = 1.0f)
{
    qreal h, s, v, a;
    base.getHsvF(&h, &s, &v, &a);
    if (s == 0.0)
        h = 0.0;
    h = std::fmod(h + h_shift + 1.0, 1.0);
    s = std::clamp(s * s_scale, 0.0, 1.0);
    v = std::clamp(v * v_scale, 0.0, 1.0);
    a = std::clamp(a * a_scale, 0.0, 1.0);

    QColor out;
    out.setHsvF(h, s, v, a);
    return out;
}

// Initialize reflectors
static void init_reflectors(std::vector<Reflect> &refs, float DX, float DY,
                            int width, int height, const QColor &tint,
                            float S)
{
    float halfW = width * 0.5f;
    float halfH = height * 0.5f;
    float matt = width;

    struct Rdef { int type; float fx, fy, fsize, r, g, b; };
    Rdef defs[] = {
        {1,  0.6699f,  0.6699f, 0.027f,   0.0f,       14/255.0f, 113/255.0f},
        {1,  0.2692f,  0.2692f, 0.010f,  90/255.0f, 181/255.0f, 142/255.0f},
        {1, -0.0112f, -0.0112f, 0.005f,  56/255.0f, 140/255.0f, 106/255.0f},
        {2,  0.6490f,  0.6490f, 0.031f,   9/255.0f,  29/255.0f,  19/255.0f},
        {2,  0.4696f,  0.4696f, 0.015f,  24/255.0f,  14/255.0f,   0.0f},
        {2,  0.4087f,  0.4087f, 0.037f,  24/255.0f,  14/255.0f,   0.0f},
        {2, -0.2003f, -0.2003f, 0.022f,  42/255.0f,  19/255.0f,   0.0f},
        {2, -0.4103f, -0.4103f, 0.025f,   0.0f,        9/255.0f,  17/255.0f},
        {2, -0.4503f, -0.4503f, 0.058f,  10/255.0f,   4/255.0f,   0.0f},
        {2, -0.5112f, -0.5112f, 0.017f,   5/255.0f,   5/255.0f,  14/255.0f},
        {2, -1.4960f, -1.4960f, 0.20f,    9/255.0f,   4/255.0f,   0.0f},
        {2, -1.4960f, -1.4960f, 0.50f,    9/255.0f,   4/255.0f,   0.0f},
        {3,  0.4487f,  0.4487f, 0.075f,  34/255.0f,  19/255.0f,   0.0f},
        {3,  1.0000f,  1.0000f, 0.10f,   14/255.0f,  26/255.0f,   0.0f},
        {3, -1.3010f, -1.3010f, 0.039f,  10/255.0f,  25/255.0f,  13/255.0f},
        {4,  1.3090f,  1.3090f, 0.19f,    9/255.0f,   0.0f,      17/255.0f},
        {4,  1.3090f,  1.3090f, 0.195f,   9/255.0f,   16/255.0f,   5/255.0f},
        {4,  1.3090f,  1.3090f, 0.20f,   17/255.0f,    4/255.0f,   0.0f},
        {4, -1.3010f, -1.3010f, 0.038f,  17/255.0f,    4/255.0f,   0.0f}
    };

    refs.clear();
    refs.reserve(std::size(defs));
    bool whiteTint = (tint.saturationF() < 0.01f);

    for (auto &d : defs) {
        Reflect r;
        r.type = d.type;
        r.size = d.fsize * matt * S;
        r.xp = halfW + d.fx * DX;
        r.yp = halfH + d.fy * DY;

        QColor base = QColor::fromRgbF(d.r, d.g, d.b, 1.0f);
        r.col = whiteTint ? base
                          : shifted_hsv(base,
                                        tint.hueF(),
                                        tint.saturationF(),
                                        tint.valueF(),
                                        tint.alphaF());
        refs.push_back(r);
    }
}

// Apply a single reflector to a pixel
static void apply_reflector(QRgb &pxl, const Reflect &r, int cx, int cy)
{
    float d = std::hypot(r.xp - cx, r.yp - cy);
    float p = 0.0f;

    switch (r.type) {
    case 1:
        p = (r.size - d) / r.size;
        if (p > 0.0f) {
            p *= p;
            pxl = blendAdd(pxl, r.col, p);
        }
        break;
    case 2:
        p = (r.size - d) / (r.size * 0.15f);
        if (p > 0.0f) {
            p = std::min(p, 1.0f);
            pxl = blendAdd(pxl, r.col, p);
        }
        break;
    case 3:
        p = (r.size - d) / (r.size * 0.12f);
        if (p > 0.0f) {
            p = std::min(p, 1.0f);
            p = 1.0f - (p * 0.12f);
            pxl = blendAdd(pxl, r.col, p);
        }
        break;
    case 4:
        p = std::abs((d - r.size) / (r.size * 0.04f));
        if (p < 1.0f) {
            pxl = blendAdd(pxl, r.col, 1.0f - p);
        }
        break;
    }
}

// Render lens flare onto the frame
std::shared_ptr<openshot::Frame>
LensFlare::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t f)
{
    auto img = frame->GetImage();
    int w = img->width();
    int h = img->height();

    // Fetch keyframe values
    float X  = x.GetValue(f),
          Y  = y.GetValue(f),
          I  = brightness.GetValue(f),
          S  = size.GetValue(f),
          SP = spread.GetValue(f);

    // Compute lens center + spread
    float halfW = w * 0.5f, halfH = h * 0.5f;
    float px    = (X * 0.5f + 0.5f) * w;
    float py    = (Y * 0.5f + 0.5f) * h;
    float DX    = (halfW - px) * SP;
    float DY    = (halfH - py) * SP;

    // Tint color
    QColor tint = QColor::fromRgbF(
        color.red.GetValue(f)   / 255.0f,
        color.green.GetValue(f) / 255.0f,
        color.blue.GetValue(f)  / 255.0f,
        color.alpha.GetValue(f) / 255.0f
    );

    // Calculate radii for rings
    float matt   = w;
    float scolor = matt * 0.0375f * S;
    float sglow  = matt * 0.078125f * S;
    float sinner = matt * 0.1796875f * S;
    float souter = matt * 0.3359375f * S;
    float shalo  = matt * 0.084375f * S;

    // Helper to tint base hues
    auto tintify = [&](float br, float bg, float bb) {
        return QColor::fromRgbF(
            br * tint.redF(),
            bg * tint.greenF(),
            bb * tint.blueF(),
            tint.alphaF()
        );
    };

    QColor c_color = tintify(239/255.0f, 239/255.0f, 239/255.0f);
    QColor c_glow  = tintify(245/255.0f, 245/255.0f, 245/255.0f);
    QColor c_inner = tintify(1.0f,       38/255.0f,   43/255.0f);
    QColor c_outer = tintify(69/255.0f,  59/255.0f,   64/255.0f);
    QColor c_halo  = tintify(80/255.0f,  15/255.0f,    4/255.0f);

    // Precompute reflectors
    std::vector<Reflect> refs;
    init_reflectors(refs, DX, DY, w, h, tint, S);

    // Build an un-premultiplied overlay
    QImage overlay(w, h, QImage::Format_ARGB32);
    overlay.fill(Qt::transparent);

    #pragma omp parallel for schedule(dynamic)
    for (int yy = 0; yy < h; ++yy) {
        QRgb *scan = reinterpret_cast<QRgb*>(overlay.scanLine(yy));
        for (int xx = 0; xx < w; ++xx) {
            // start fully transparent
            int r=0, g=0, b=0;
            float d = std::hypot(xx - px, yy - py);

            // bright core
            if (d < scolor) {
                float p = (scolor - d)/scolor; p*=p;
                QRgb tmp = blendAdd(qRgba(r,g,b,0), c_color, p);
                r = qRed(tmp); g = qGreen(tmp); b = qBlue(tmp);
            }
            // outer glow
            if (d < sglow) {
                float p = (sglow - d)/sglow; p*=p;
                QRgb tmp = blendAdd(qRgba(r,g,b,0), c_glow, p);
                r = qRed(tmp); g = qGreen(tmp); b = qBlue(tmp);
            }
            // inner ring
            if (d < sinner) {
                float p = (sinner - d)/sinner; p*=p;
                QRgb tmp = blendAdd(qRgba(r,g,b,0), c_inner, p);
                r = qRed(tmp); g = qGreen(tmp); b = qBlue(tmp);
            }
            // outer ring
            if (d < souter) {
                float p = (souter - d)/souter;
                QRgb tmp = blendAdd(qRgba(r,g,b,0), c_outer, p);
                r = qRed(tmp); g = qGreen(tmp); b = qBlue(tmp);
            }
            // halo ring
            {
                float p = std::abs((d - shalo)/(shalo*0.07f));
                if (p < 1.0f) {
                    QRgb tmp = blendAdd(qRgba(r,g,b,0), c_halo, 1.0f-p);
                    r = qRed(tmp); g = qGreen(tmp); b = qBlue(tmp);
                }
            }
            // little reflectors
            for (auto &rf : refs) {
                QRgb tmp = qRgba(r,g,b,0);
                apply_reflector(tmp, rf, xx, yy);
                r = qRed(tmp); g = qGreen(tmp); b = qBlue(tmp);
            }

            // force alpha = max(R,G,B)
            int a = std::max({r,g,b});
            scan[xx] = qRgba(r,g,b,a);
        }
    }

    // Get original alpha
    QImage origAlpha = img->convertToFormat(QImage::Format_Alpha8);

    // Additive-light the overlay onto your frame
    QPainter p(img.get());
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    p.setOpacity(I);
    p.drawImage(0, 0, overlay);
    p.end();

    // Rebuild alpha = max(orig, flare×I)
    QImage finalA(w,h, QImage::Format_Alpha8);
    auto overlayA = overlay.convertToFormat(QImage::Format_Alpha8);

    for (int yy=0; yy<h; ++yy) {
        uchar *oL = origAlpha.scanLine(yy);
        uchar *fL = overlayA.scanLine(yy);
        uchar *nL = finalA.scanLine(yy);
        for (int xx=0; xx<w; ++xx) {
            float oa = oL[xx]/255.0f;
            float fa = (fL[xx]/255.0f)*I;
            nL[xx] = static_cast<uchar>(std::clamp(std::max(oa,fa)*255.0f, 0.0f, 255.0f));
        }
    }
    img->setAlphaChannel(finalA);
    return frame;
}

// Create a new frame for this effect
std::shared_ptr<openshot::Frame>
LensFlare::GetFrame(int64_t frame_number)
{
    return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
}

// Convert effect to JSON string
std::string LensFlare::Json() const
{
    return JsonValue().toStyledString();
}

// Convert effect to JSON value
Json::Value LensFlare::JsonValue() const
{
    Json::Value r = EffectBase::JsonValue();
    r["type"] = info.class_name;
    r["x"] = x.JsonValue();
    r["y"] = y.JsonValue();
    r["brightness"] = brightness.JsonValue();
    r["size"] = size.JsonValue();
    r["spread"] = spread.JsonValue();
    r["color"] = color.JsonValue();
    return r;
}

// Parse JSON from string
void LensFlare::SetJson(const std::string v)
{
    try { SetJsonValue(openshot::stringToJson(v)); }
    catch (...) { throw InvalidJSON("LensFlare JSON"); }
}

// Apply JSON values to effect
void LensFlare::SetJsonValue(const Json::Value r)
{
    EffectBase::SetJsonValue(r);
    if (!r["x"].isNull()) x.SetJsonValue(r["x"]);
    if (!r["y"].isNull()) y.SetJsonValue(r["y"]);
    if (!r["brightness"].isNull()) brightness.SetJsonValue(r["brightness"]);
    if (!r["size"].isNull()) size.SetJsonValue(r["size"]);
    if (!r["spread"].isNull()) spread.SetJsonValue(r["spread"]);
    if (!r["color"].isNull()) color.SetJsonValue(r["color"]);
}

// Get properties as JSON for UI
std::string LensFlare::PropertiesJSON(int64_t f) const
{
    Json::Value r = BasePropertiesJSON(f);
    r["x"] = add_property_json("X", x.GetValue(f), "float", "-1..1", &x, -1, 1, false, f);
    r["y"] = add_property_json("Y", y.GetValue(f), "float", "-1..1", &y, -1, 1, false, f);
    r["brightness"] = add_property_json("Brightness", brightness.GetValue(f), "float", "0..1", &brightness, 0, 1, false, f);
    r["size"] = add_property_json("Size", size.GetValue(f), "float", "0.1..3", &size, 0.1, 3, false, f);
    r["spread"] = add_property_json("Spread", spread.GetValue(f), "float", "0..1", &spread, 0, 1, false, f);
    r["color"] = add_property_json("Tint Color", 0.0, "color", "", &color.red, 0, 255, false, f);
    r["color"]["red"] = add_property_json("Red", color.red.GetInt(f), "float", "0..255", &color.red, 0, 255, false, f);
    r["color"]["green"] = add_property_json("Green", color.green.GetInt(f), "float", "0..255", &color.green, 0, 255, false, f);
    r["color"]["blue"] = add_property_json("Blue", color.blue.GetInt(f), "float", "0..255", &color.blue, 0, 255, false, f);
    r["color"]["alpha"] = add_property_json("Alpha", color.alpha.GetInt(f), "float", "0..255", &color.alpha, 0, 255, false, f);
    return r.toStyledString();
}
