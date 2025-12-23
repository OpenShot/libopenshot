/**
 * @file
 * @brief Unit tests for openshot::SphericalProjection using PNG fixtures
 * @author Jonathan Thomas
 *
 * @ref License
 *
 * Copyright (c) 2008-2025 OpenShot Studios, LLC
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "Frame.h"
#include "effects/SphericalProjection.h"
#include "openshot_catch.h"

#include <QColor>
#include <QImage>
#include <memory>
#include <cmath>
#include <algorithm>

using namespace openshot;

// Pretty-print QColor on failure
static std::ostream &operator<<(std::ostream &os, QColor const &c) {
  os << "QColor(" << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";
  return os;
}

// Load a PNG fixture into a fresh Frame
static std::shared_ptr<Frame> loadFrame(const char *filename) {
  QImage img(QString(TEST_MEDIA_PATH) + filename);
  img = img.convertToFormat(QImage::Format_ARGB32);
  auto f = std::make_shared<Frame>();
  *f->GetImage() = img;
  return f;
}

// Helpers to sample pixels
static QColor centerPixel(SphericalProjection &e, std::shared_ptr<Frame> f) {
  auto img = e.GetFrame(f, 1)->GetImage();
  int cx = img->width() / 2;
  int cy = img->height() / 2;
  return img->pixelColor(cx, cy);
}

static QColor offsetPixel(std::shared_ptr<QImage> img, int dx, int dy) {
  const int cx = img->width() / 2 + dx;
  const int cy = img->height() / 2 + dy;
  return img->pixelColor(std::clamp(cx, 0, img->width() - 1),
                         std::clamp(cy, 0, img->height() - 1));
}

// Loose classifiers for our colored guide lines
static bool is_red(QColor c)    { return c.red()   >= 200 && c.green() <= 60 && c.blue() <= 60; }
static bool is_yellow(QColor c) { return c.red()   >= 200 && c.green() >= 170 && c.blue() <= 60; }

/* ----------------------------------------------------------------------------
 * Invert behavior vs Yaw+180 (Equirect input)
 * ----------------------------------------------------------------------------
 * In both RECT_SPHERE and RECT_HEMISPHERE, Invert should match adding 180° of
 * yaw (no mirroring). Compare the center pixel using *fresh* inputs.
 */

TEST_CASE("sphere mode: invert equals yaw+180 (center pixel)", "[effect][spherical]") {
  // A: invert=BACK, yaw=0
  SphericalProjection eA;
  eA.input_model = SphericalProjection::INPUT_EQUIRECT;
  eA.projection_mode = SphericalProjection::MODE_RECT_SPHERE;
  eA.in_fov = Keyframe(180.0);
  eA.fov    = Keyframe(90.0);
  eA.interpolation = SphericalProjection::INTERP_NEAREST;
  eA.invert = SphericalProjection::INVERT_BACK;
  eA.yaw = Keyframe(0.0);

  // B: invert=NORMAL, yaw=180
  SphericalProjection eB = eA;
  eB.invert = SphericalProjection::INVERT_NORMAL;
  eB.yaw = Keyframe(180.0);

  auto fA = loadFrame("eq_sphere.png");
  auto fB = loadFrame("eq_sphere.png");

  CHECK(centerPixel(eA, fA) == centerPixel(eB, fB));
}

TEST_CASE("hemisphere mode: invert equals yaw+180 (center pixel)", "[effect][spherical]") {
  // A: invert=BACK, yaw=0
  SphericalProjection eA;
  eA.input_model = SphericalProjection::INPUT_EQUIRECT;
  eA.projection_mode = SphericalProjection::MODE_RECT_HEMISPHERE;
  eA.in_fov = Keyframe(180.0);
  eA.fov    = Keyframe(90.0);
  eA.interpolation = SphericalProjection::INTERP_NEAREST;
  eA.invert = SphericalProjection::INVERT_BACK;
  eA.yaw = Keyframe(0.0);

  // B: invert=NORMAL, yaw=180
  SphericalProjection eB = eA;
  eB.invert = SphericalProjection::INVERT_NORMAL;
  eB.yaw = Keyframe(180.0);

  auto fA = loadFrame("eq_sphere.png");
  auto fB = loadFrame("eq_sphere.png");

  CHECK(centerPixel(eA, fA) == centerPixel(eB, fB));
}

/* ----------------------------------------------------------------------------
 * Fisheye input: center pixel should be invariant to yaw/invert
 * ----------------------------------------------------------------------------
 */

TEST_CASE("fisheye input: center pixel invariant under invert", "[effect][spherical]") {
  SphericalProjection base;
  base.input_model = SphericalProjection::INPUT_FEQ_EQUIDISTANT;
  base.projection_mode = SphericalProjection::MODE_RECT_SPHERE;
  base.in_fov = Keyframe(180.0);
  base.fov = Keyframe(180.0);
  base.interpolation = SphericalProjection::INTERP_NEAREST;

  // Baseline
  SphericalProjection e0 = base;
  e0.invert = SphericalProjection::INVERT_NORMAL;
  e0.yaw = Keyframe(0.0);
  QColor c0 = centerPixel(e0, loadFrame("fisheye.png"));

  // Invert
  SphericalProjection e1 = base;
  e1.invert = SphericalProjection::INVERT_BACK;
  e1.yaw = Keyframe(0.0);
  QColor c1 = centerPixel(e1, loadFrame("fisheye.png"));

  // Yaw +45 should point elsewhere
  SphericalProjection e2 = base;
  e2.invert = SphericalProjection::INVERT_NORMAL;
  e2.yaw = Keyframe(45.0);
  QColor c2 = centerPixel(e2, loadFrame("fisheye.png"));

  CHECK(c0 == c1);
  CHECK(c0 != c2);
}

/* ----------------------------------------------------------------------------
 * Cache invalidation sanity check
 * ----------------------------------------------------------------------------
 */

TEST_CASE("changing properties invalidates cache", "[effect][spherical]") {
  SphericalProjection e;
  e.input_model = SphericalProjection::INPUT_EQUIRECT;
  e.projection_mode = SphericalProjection::MODE_RECT_SPHERE;
  e.yaw = Keyframe(45.0);
  e.invert = SphericalProjection::INVERT_NORMAL;
  e.interpolation = SphericalProjection::INTERP_NEAREST;

  QColor c0 = centerPixel(e, loadFrame("eq_sphere.png"));
  e.invert = SphericalProjection::INVERT_BACK; // should rebuild UV map
  QColor c1 = centerPixel(e, loadFrame("eq_sphere.png"));

  CHECK(c1 != c0);
}

/* ----------------------------------------------------------------------------
 * Checker-plane fixtures (rectilinear output)
 * ----------------------------------------------------------------------------
 * Validate the colored guide lines (red vertical meridian at center, yellow
 * equator horizontally). We use tolerant classifiers to avoid brittle
 * single-pixel mismatches.
 */

TEST_CASE("input models: checker-plane colored guides are consistent", "[effect][spherical]") {
  SphericalProjection e;
  e.projection_mode = SphericalProjection::MODE_RECT_SPHERE;
  e.fov = Keyframe(90.0);
  e.in_fov = Keyframe(180.0);
  e.yaw = Keyframe(0.0);
  e.pitch = Keyframe(0.0);
  e.roll = Keyframe(0.0);
  e.interpolation = SphericalProjection::INTERP_NEAREST;

  auto check_guides = [&](int input_model, const char *file) {
    e.input_model = input_model;
    auto out = e.GetFrame(loadFrame(file), 1)->GetImage();

    // Center column should hit the red meridian (allow 1px tolerance)
    // Sample above the equator to avoid overlap with the yellow line
    bool center_red = false;
    for (int dx = -5; dx <= 5 && !center_red; ++dx)
      center_red = center_red || is_red(offsetPixel(out, dx, -60));
    REQUIRE(center_red);

    // A bit left/right along the equator should be yellow
    CHECK(is_yellow(offsetPixel(out, -60, 0)));
    CHECK(is_yellow(offsetPixel(out,  60, 0)));
  };

  SECTION("equirect input") {
    check_guides(SphericalProjection::INPUT_EQUIRECT, "eq_sphere_plane.png");
  }
  SECTION("fisheye equidistant input") {
    check_guides(SphericalProjection::INPUT_FEQ_EQUIDISTANT, "fisheye_plane_equidistant.png");
  }
  SECTION("fisheye equisolid input") {
    check_guides(SphericalProjection::INPUT_FEQ_EQUISOLID, "fisheye_plane_equisolid.png");
  }
  SECTION("fisheye stereographic input") {
    check_guides(SphericalProjection::INPUT_FEQ_STEREOGRAPHIC, "fisheye_plane_stereographic.png");
  }
  SECTION("fisheye orthographic input") {
    check_guides(SphericalProjection::INPUT_FEQ_ORTHOGRAPHIC, "fisheye_plane_orthographic.png");
  }
}

/* ----------------------------------------------------------------------------
 * Fisheye output modes from equirect plane
 * ----------------------------------------------------------------------------
 * - Center pixel should match the rect view's center (same yaw).
 * - Corners are outside the fisheye disk and should be fully transparent.
 */

TEST_CASE("output fisheye modes: center matches rect view, corners outside disk", "[effect][spherical]") {
  // Expected center color using rectilinear view
  SphericalProjection rect;
  rect.input_model = SphericalProjection::INPUT_EQUIRECT;
  rect.projection_mode = SphericalProjection::MODE_RECT_SPHERE;
  rect.in_fov = Keyframe(180.0);
  rect.fov = Keyframe(90.0);
  rect.interpolation = SphericalProjection::INTERP_NEAREST;
  QColor expected_center = centerPixel(rect, loadFrame("eq_sphere_plane.png"));

  auto verify_mode = [&](int mode) {
    SphericalProjection e;
    e.input_model = SphericalProjection::INPUT_EQUIRECT;
    e.projection_mode = mode;            // one of the fisheye outputs
    e.in_fov = Keyframe(180.0);
    e.fov = Keyframe(180.0);
    e.interpolation = SphericalProjection::INTERP_NEAREST;

    auto img = e.GetFrame(loadFrame("eq_sphere_plane.png"), 1)->GetImage();

    // Center matches rect view
    CHECK(is_red(expected_center) == is_red(offsetPixel(img, 0, 0)));

    // Corners are fully outside disk => transparent black
    QColor transparent(0,0,0,0);
    QColor tl = offsetPixel(img, -img->width()/2 + 2, -img->height()/2 + 2);
    QColor tr = offsetPixel(img,  img->width()/2 - 2, -img->height()/2 + 2);
    QColor bl = offsetPixel(img, -img->width()/2 + 2,  img->height()/2 - 2);
    QColor br = offsetPixel(img,  img->width()/2 - 2,  img->height()/2 - 2);

    CHECK(tl == transparent);
    CHECK(tr == transparent);
    CHECK(bl == transparent);
    CHECK(br == transparent);
  };

  verify_mode(SphericalProjection::MODE_FISHEYE_EQUIDISTANT);
  verify_mode(SphericalProjection::MODE_FISHEYE_EQUISOLID);
  verify_mode(SphericalProjection::MODE_FISHEYE_STEREOGRAPHIC);
  verify_mode(SphericalProjection::MODE_FISHEYE_ORTHOGRAPHIC);
}
