/**
 * @file
 * @brief Unit tests for openshot::Wave effect
 * @author OpenAI ChatGPT
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <QImage>
#include <QColor>

#include "Frame.h"
#include "effects/Wave.h"
#include "openshot_catch.h"

using namespace openshot;

TEST_CASE("Wave uses original pixel buffer", "[effect][wave]")
{
	// Create 1x10 image with increasing red channel
	QImage img(10, 1, QImage::Format_ARGB32);
	for (int x = 0; x < 10; ++x)
		img.setPixelColor(x, 0, QColor(x, 0, 0, 255));
	auto f = std::make_shared<Frame>();
	*f->GetImage() = img;

	Wave w;
	w.wavelength = Keyframe(0.0);
	w.amplitude = Keyframe(1.0);
	w.multiplier = Keyframe(0.01);
	w.shift_x = Keyframe(-1.0); // negative shift to copy from previous pixel
	w.speed_y = Keyframe(0.0);

	auto out_img = w.GetFrame(f, 1)->GetImage();
	int expected[10] = {0,0,1,2,3,4,5,6,7,8};
	for (int x = 0; x < 10; ++x)
		CHECK(out_img->pixelColor(x,0).red() == expected[x]);
}


using namespace openshot;

static std::shared_ptr<Frame> MakeWaveFrame()
{
    auto frame = std::make_shared<Frame>();
    QImage image(7, 7, QImage::Format_ARGB32);

    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            image.setPixelColor(x, y, QColor(x * 30, y * 30, 0));

    *frame->GetImage() = image;
    return frame;
}

static Wave MakeDirectionalWave(int mode)
{
    Wave wave;
    wave.direction = Keyframe(mode);
    wave.wavelength = Keyframe(0.0);
    wave.amplitude = Keyframe(1.0);
    wave.multiplier = Keyframe(0.01);
    wave.shift_x = Keyframe(1.0);
    wave.speed_y = Keyframe(0.0);
    return wave;
}

TEST_CASE("Wave horizontal directional displacement", "[effect][wave]")
{
    Wave wave = MakeDirectionalWave(1);
    auto image = wave.GetFrame(MakeWaveFrame(), 1)->GetImage();

    CHECK(image->pixelColor(3, 3) == QColor(120, 90, 0));
}

TEST_CASE("Wave vertical directional displacement", "[effect][wave]")
{
    Wave wave = MakeDirectionalWave(2);
    auto image = wave.GetFrame(MakeWaveFrame(), 1)->GetImage();

    CHECK(image->pixelColor(3, 3) == QColor(90, 120, 0));
}

TEST_CASE("Wave angled displacement", "[effect][wave]")
{
    Wave wave = MakeDirectionalWave(3);
    wave.displacement_angle = Keyframe(90.0);

    auto image = wave.GetFrame(MakeWaveFrame(), 1)->GetImage();

    CHECK(image->pixelColor(3, 3) == QColor(90, 120, 0));
}

TEST_CASE("Wave directional JSON round trip", "[effect][wave]")
{
    Wave wave;
    wave.direction = Keyframe(3.0);
    wave.warp_angle = Keyframe(45.0);
    wave.displacement_angle = Keyframe(90.0);

    Wave restored;
    restored.SetJson(wave.Json());

    CHECK(restored.direction.GetValue(1) == 3.0);
    CHECK(restored.warp_angle.GetValue(1) == 45.0);
    CHECK(restored.displacement_angle.GetValue(1) == 90.0);
}

TEST_CASE("Wave legacy JSON defaults", "[effect][wave]")
{
    Wave wave;
    Json::Value legacy = wave.JsonValue();

    legacy.removeMember("direction");
    legacy.removeMember("warp_angle");
    legacy.removeMember("displacement_angle");

    Wave restored;
    restored.SetJsonValue(legacy);

    CHECK(restored.direction.GetValue(1) == 0.0);
    CHECK(restored.warp_angle.GetValue(1) == 0.0);
    CHECK(restored.displacement_angle.GetValue(1) == 0.0);
}

TEST_CASE("Wave warp angle changes pattern orientation", "[effect][wave]")
{
    Wave wave = MakeDirectionalWave(1);
    wave.wavelength = Keyframe(1.5707963267948966);
    wave.shift_x = Keyframe(0.0);

    wave.warp_angle = Keyframe(0.0);
    auto vertical_pattern = wave.GetFrame(MakeWaveFrame(), 1)->GetImage();

    wave.warp_angle = Keyframe(90.0);
    auto horizontal_pattern = wave.GetFrame(MakeWaveFrame(), 1)->GetImage();

    CHECK(vertical_pattern->pixelColor(3, 4) == QColor(120, 120, 0));
    CHECK(horizontal_pattern->pixelColor(3, 4) == QColor(90, 120, 0));
}

TEST_CASE("Wave direction supports animated keyframes", "[effect][wave]")
{
    Wave wave = MakeDirectionalWave(1);
    wave.direction = Keyframe();

    wave.direction.AddPoint(1, 1.0);
    wave.direction.AddPoint(10, 2.0);

    auto first = wave.GetFrame(MakeWaveFrame(), 1)->GetImage();
    auto last = wave.GetFrame(MakeWaveFrame(), 10)->GetImage();

    CHECK(first->pixelColor(3, 3) == QColor(120, 90, 0));
    CHECK(last->pixelColor(3, 3) == QColor(90, 120, 0));
}

TEST_CASE("Wave directional displacement clamps image boundaries", "[effect][wave]")
{
    Wave wave = MakeDirectionalWave(1);
    wave.shift_x = Keyframe(100.0);

    auto image = wave.GetFrame(MakeWaveFrame(), 1)->GetImage();

    CHECK(image->pixelColor(3, 3) == QColor(180, 90, 0));
    CHECK(image->pixelColor(6, 6) == QColor(180, 180, 0));
}


TEST_CASE("Wave directional property metadata", "[effect][wave]")
{
    Wave wave;
    wave.direction = Keyframe(3.0);
    wave.warp_angle = Keyframe(45.0);
    wave.displacement_angle = Keyframe(90.0);

    Json::Value properties;
    Json::Reader reader;

    REQUIRE(reader.parse(wave.PropertiesJSON(1), properties));

    REQUIRE(properties.isMember("direction"));
    REQUIRE(properties.isMember("warp_angle"));
    REQUIRE(properties.isMember("displacement_angle"));

    CHECK(properties["direction"]["value"].asDouble() == 3.0);
    CHECK(properties["warp_angle"]["value"].asDouble() == 45.0);
    CHECK(properties["displacement_angle"]["value"].asDouble() == 90.0);
}

TEST_CASE("Wave directional mode handles empty image", "[effect][wave]")
{
    Wave wave = MakeDirectionalWave(1);

    auto frame = std::make_shared<Frame>();
    auto image = frame->GetImage();

    REQUIRE(image != nullptr);
    *image = QImage();

    auto result = wave.GetFrame(frame, 1);

    CHECK(result == frame);
    CHECK(result->GetImage()->isNull());
}

TEST_CASE("Wave parameterized constructor preserves legacy defaults", "[effect][wave]")
{
    Wave wave(
        Keyframe(0.1),
        Keyframe(0.5),
        Keyframe(0.2),
        Keyframe(2.0),
        Keyframe(0.3)
    );

    CHECK(wave.wavelength.GetValue(1) == Catch::Approx(0.1));
    CHECK(wave.amplitude.GetValue(1) == 0.5);
    CHECK(wave.multiplier.GetValue(1) == Catch::Approx(0.2));
    CHECK(wave.shift_x.GetValue(1) == 2.0);
    CHECK(wave.speed_y.GetValue(1) == Catch::Approx(0.3));

    CHECK(wave.direction.GetValue(1) == 0.0);
    CHECK(wave.warp_angle.GetValue(1) == 0.0);
    CHECK(wave.displacement_angle.GetValue(1) == 0.0);
}
