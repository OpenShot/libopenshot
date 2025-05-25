/**
* @file
 * @brief Source file for Sharpen class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later


#include "Sharpen.h"
#include "Exceptions.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <omp.h>

using namespace openshot;

// Constructor with default keyframes
Sharpen::Sharpen()
  : amount(10.0)
  , radius(3.0)
  , threshold(0.0)
  , mode(0)
  , channel(1)
{
  init_effect_details();
}

// Constructor from keyframes
Sharpen::Sharpen(Keyframe a, Keyframe r, Keyframe t)
  : amount(a)
  , radius(r)
  , threshold(t)
  , mode(0)
  , channel(1)
{
  init_effect_details();
}

// Initialize effect metadata
void Sharpen::init_effect_details()
{
  InitEffectInfo();
  info.class_name = "Sharpen";
  info.name        = "Sharpen";
  info.description = "Boost edge contrast to make video details look crisper.";
  info.has_audio   = false;
  info.has_video   = true;
}

// Compute three box sizes to approximate a Gaussian of sigma
static void boxes_for_gauss(double sigma, int b[3])
{
  const int n = 3;
  double wi = std::sqrt((12.0 * sigma * sigma / n) + 1.0);
  int wl = int(std::floor(wi));
  if (!(wl & 1)) --wl;
  int wu = wl + 2;
  double mi = (12.0 * sigma * sigma - n*wl*wl - 4.0*n*wl - 3.0*n)
              / (-4.0*wl - 4.0);
  int m = int(std::round(mi));
  for (int i = 0; i < n; ++i)
    b[i] = i < m ? wl : wu;
}

// Blur one axis with an edge-replicate sliding window
static void blur_axis(const QImage& src, QImage& dst, int r, bool vertical)
{
  if (r <= 0) {
    dst = src.copy();
    return;
  }

  int W   = src.width();
  int H   = src.height();
  int bpl = src.bytesPerLine();
  const uchar* in  = src.bits();
  uchar*       out = dst.bits();
  int window = 2*r + 1;

  if (!vertical) {
    #pragma omp parallel for
    for (int y = 0; y < H; ++y) {
      const uchar* rowIn  = in  + y*bpl;
      uchar*       rowOut = out + y*bpl;
      double sB = rowIn[0]*(r+1), sG = rowIn[1]*(r+1),
             sR = rowIn[2]*(r+1), sA = rowIn[3]*(r+1);
      for (int x = 1; x <= r; ++x) {
        const uchar* p = rowIn + std::min(x, W-1)*4;
        sB += p[0]; sG += p[1]; sR += p[2]; sA += p[3];
      }
      for (int x = 0; x < W; ++x) {
        uchar* o = rowOut + x*4;
        o[0] = uchar(sB / window + 0.5);
        o[1] = uchar(sG / window + 0.5);
        o[2] = uchar(sR / window + 0.5);
        o[3] = uchar(sA / window + 0.5);

        const uchar* addP = rowIn + std::min(x+r+1, W-1)*4;
        const uchar* subP = rowIn + std::max(x-r,     0)*4;
        sB += addP[0] - subP[0];
        sG += addP[1] - subP[1];
        sR += addP[2] - subP[2];
        sA += addP[3] - subP[3];
      }
    }
  }
  else {
    #pragma omp parallel for
    for (int x = 0; x < W; ++x) {
      double sB = 0, sG = 0, sR = 0, sA = 0;
      const uchar* p0 = in + x*4;
      sB = p0[0]*(r+1); sG = p0[1]*(r+1);
      sR = p0[2]*(r+1); sA = p0[3]*(r+1);
      for (int y = 1; y <= r; ++y) {
        const uchar* p = in + std::min(y, H-1)*bpl + x*4;
        sB += p[0]; sG += p[1]; sR += p[2]; sA += p[3];
      }
      for (int y = 0; y < H; ++y) {
        uchar* o = out + y*bpl + x*4;
        o[0] = uchar(sB / window + 0.5);
        o[1] = uchar(sG / window + 0.5);
        o[2] = uchar(sR / window + 0.5);
        o[3] = uchar(sA / window + 0.5);

        const uchar* addP = in + std::min(y+r+1, H-1)*bpl + x*4;
        const uchar* subP = in + std::max(y-r,     0)*bpl + x*4;
        sB += addP[0] - subP[0];
        sG += addP[1] - subP[1];
        sR += addP[2] - subP[2];
        sA += addP[3] - subP[3];
      }
    }
  }
}

// Wrapper to handle fractional radius by blending two integer passes
static void box_blur(const QImage& src, QImage& dst, double rf, bool vertical)
{
  int r0 = int(std::floor(rf));
  int r1 = r0 + 1;
  double f = rf - r0;
  if (f < 1e-4) {
    blur_axis(src, dst, r0, vertical);
  }
  else {
    QImage a(src.size(), QImage::Format_ARGB32);
    QImage b(src.size(), QImage::Format_ARGB32);
    blur_axis(src, a, r0, vertical);
    blur_axis(src, b, r1, vertical);

    int pixels = src.width() * src.height();
    const uchar* pa = a.bits();
    const uchar* pb = b.bits();
    uchar*       pd = dst.bits();
    #pragma omp parallel for
    for (int i = 0; i < pixels; ++i) {
      for (int c = 0; c < 4; ++c) {
        pd[i*4+c] = uchar((1.0 - f) * pa[i*4+c]
                        + f         * pb[i*4+c]
                        + 0.5);
      }
    }
  }
}

// Apply three sequential box blurs to approximate Gaussian
static void gauss_blur(const QImage& src, QImage& dst, double sigma)
{
  int b[3];
  boxes_for_gauss(sigma, b);
  QImage t1(src.size(), QImage::Format_ARGB32);
  QImage t2(src.size(), QImage::Format_ARGB32);

  double r = 0.5 * (b[0] - 1);
  box_blur(src , t1, r, false);
  box_blur(t1, t2, r, true);

  r = 0.5 * (b[1] - 1);
  box_blur(t2, t1, r, false);
  box_blur(t1, t2, r, true);

  r = 0.5 * (b[2] - 1);
  box_blur(t2, t1, r, false);
  box_blur(t1, dst, r, true);
}

// Main frame processing
std::shared_ptr<Frame> Sharpen::GetFrame(
  std::shared_ptr<Frame> frame, int64_t frame_number)
{
  auto img = frame->GetImage();
  if (!img || img->isNull())
    return frame;
  if (img->format() != QImage::Format_ARGB32)
    *img = img->convertToFormat(QImage::Format_ARGB32);

  int W = img->width();
  int H = img->height();
  if (W <= 0 || H <= 0)
    return frame;

  // Retrieve keyframe values
  double amt   = amount.GetValue(frame_number);    // 0–40
  double rpx   = radius.GetValue(frame_number);    // px
  double thrUI = threshold.GetValue(frame_number); // 0–1

  // Sigma scaled against 720p reference
  double sigma = std::max(0.1, rpx * H / 720.0);

  // Generate blurred image
  QImage blur(W, H, QImage::Format_ARGB32);
  gauss_blur(*img, blur, sigma);

  // Precompute maximum luma difference for adaptive threshold
  int bplS = img->bytesPerLine();
  int bplB = blur.bytesPerLine();
  uchar* sBits = img->bits();
  uchar* bBits = blur.bits();

  double maxDY = 0.0;
  #pragma omp parallel for reduction(max:maxDY)
  for (int y = 0; y < H; ++y) {
    uchar* sRow = sBits + y * bplS;
    uchar* bRow = bBits + y * bplB;
    for (int x = 0; x < W; ++x) {
      double dB = double(sRow[x*4+0]) - double(bRow[x*4+0]);
      double dG = double(sRow[x*4+1]) - double(bRow[x*4+1]);
      double dR = double(sRow[x*4+2]) - double(bRow[x*4+2]);
      double dY = std::abs(0.114*dB + 0.587*dG + 0.299*dR);
      maxDY = std::max(maxDY, dY);
    }
  }

  // Compute actual threshold in luma units
  double thr = thrUI * maxDY;

  // Process pixels
  #pragma omp parallel for
  for (int y = 0; y < H; ++y) {
    uchar* sRow = sBits + y * bplS;
    uchar* bRow = bBits + y * bplB;
    for (int x = 0; x < W; ++x) {
      uchar* sp = sRow + x*4;
      uchar* bp = bRow + x*4;

      // Detail per channel
      double dB = double(sp[0]) - double(bp[0]);
      double dG = double(sp[1]) - double(bp[1]);
      double dR = double(sp[2]) - double(bp[2]);
      double dY = 0.114*dB + 0.587*dG + 0.299*dR;

      // Skip if below adaptive threshold
      if (std::abs(dY) < thr)
        continue;

      // Halo limiter
      auto halo = [](double d) {
        return (255.0 - std::abs(d)) / 255.0;
      };

      double outC[3];

      if (mode == 1) {
        // HighPass: base = blurred image
        // detail = original – blurred
        // no halo limiter

        // precompute normalized luma weights
        const double wB = 0.114, wG = 0.587, wR = 0.299;

        if (channel == 1) {
          // Luma only: add back luma detail weighted per channel
          double lumaInc = amt * dY;
          outC[0] = bp[0] + lumaInc * wB;
          outC[1] = bp[1] + lumaInc * wG;
          outC[2] = bp[2] + lumaInc * wR;
        }
        else if (channel == 2) {
          // Chroma only: subtract luma from detail, add chroma back
          double lumaDetail = dY;
          double chromaB    = dB - lumaDetail * wB;
          double chromaG    = dG - lumaDetail * wG;
          double chromaR    = dR - lumaDetail * wR;
          outC[0] = bp[0] + amt * chromaB;
          outC[1] = bp[1] + amt * chromaG;
          outC[2] = bp[2] + amt * chromaR;
        }
        else {
          // All channels: add full per-channel detail
          outC[0] = bp[0] + amt * dB;
          outC[1] = bp[1] + amt * dG;
          outC[2] = bp[2] + amt * dR;
        }
      }
      else {
        // Unsharp-Mask: base = original + amt * detail * halo(detail)
        if (channel == 1) {
          // Luma only
          double inc = amt * dY * halo(dY);
          for (int c = 0; c < 3; ++c)
            outC[c] = sp[c] + inc;
        }
        else if (channel == 2) {
          // Chroma only
          double l = dY;
          double chroma[3] = { dB - l, dG - l, dR - l };
          for (int c = 0; c < 3; ++c)
            outC[c] = sp[c] + amt * chroma[c] * halo(chroma[c]);
        }
        else {
          // All channels
          outC[0] = sp[0] + amt * dB * halo(dB);
          outC[1] = sp[1] + amt * dG * halo(dG);
          outC[2] = sp[2] + amt * dR * halo(dR);
        }
      }

      // Write back clamped
      for (int c = 0; c < 3; ++c) {
        sp[c] = uchar(std::clamp(outC[c], 0.0, 255.0) + 0.5);
      }
    }
  }

  return frame;
}

// JSON serialization
std::string Sharpen::Json() const
{
  return JsonValue().toStyledString();
}

Json::Value Sharpen::JsonValue() const
{
  Json::Value root = EffectBase::JsonValue();
  root["type"]      = info.class_name;
  root["amount"]    = amount.JsonValue();
  root["radius"]    = radius.JsonValue();
  root["threshold"] = threshold.JsonValue();
  root["mode"]      = mode;
  root["channel"]   = channel;
  return root;
}

// JSON deserialization
void Sharpen::SetJson(std::string value)
{
  auto root = openshot::stringToJson(value);
  SetJsonValue(root);
}

void Sharpen::SetJsonValue(Json::Value root)
{
  EffectBase::SetJsonValue(root);
  if (!root["amount"].isNull())
    amount.SetJsonValue(root["amount"]);
  if (!root["radius"].isNull())
    radius.SetJsonValue(root["radius"]);
  if (!root["threshold"].isNull())
    threshold.SetJsonValue(root["threshold"]);
  if (!root["mode"].isNull())
    mode    = root["mode"].asInt();
  if (!root["channel"].isNull())
    channel = root["channel"].asInt();
}

// UI property definitions
std::string Sharpen::PropertiesJSON(int64_t t) const
{
  Json::Value root = BasePropertiesJSON(t);
  root["amount"] = add_property_json(
    "Amount", amount.GetValue(t), "float", "", &amount, 0, 40, false, t);
  root["radius"] = add_property_json(
    "Radius", radius.GetValue(t), "float", "pixels", &radius, 0, 10, false, t);
  root["threshold"] = add_property_json(
    "Threshold", threshold.GetValue(t), "float", "ratio", &threshold, 0, 1, false, t);
  root["mode"] = add_property_json(
    "Mode", mode, "int", "", nullptr, 0, 1, false, t);
  root["mode"]["choices"].append(add_property_choice_json("UnsharpMask",   0, mode));
  root["mode"]["choices"].append(add_property_choice_json("HighPassBlend", 1, mode));
  root["channel"] = add_property_json(
    "Channel", channel, "int", "", nullptr, 0, 2, false, t);
  root["channel"]["choices"].append(add_property_choice_json("All",    0, channel));
  root["channel"]["choices"].append(add_property_choice_json("Luma",   1, channel));
  root["channel"]["choices"].append(add_property_choice_json("Chroma", 2, channel));
  return root.toStyledString();
}
