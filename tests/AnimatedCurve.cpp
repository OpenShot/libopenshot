/**
 * @file
 * @brief Unit tests for AnimatedCurve classes
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AnimatedCurve.h"
#include "openshot_catch.h"

using namespace openshot;

TEST_CASE("AnimatedCurve defaults to identity curve", "[animatedcurve]")
{
	AnimatedCurve curve;

	CHECK(curve.Nodes().size() == 2);
	CHECK(curve.Sample(0.0f, 1) == Approx(0.0f));
	CHECK(curve.Sample(0.5f, 1) == Approx(0.5f).margin(0.01f));
	CHECK(curve.Sample(1.0f, 1) == Approx(1.0f));
}

TEST_CASE("AnimatedCurve interpolates node coordinates over time", "[animatedcurve][animation]")
{
	AnimatedCurve curve;
	curve.Nodes().clear();

	AnimatedCurveNode left(10, 0.0, 0.0, LINEAR);
	left.y.AddPoint(300, 0.5, LINEAR);
	AnimatedCurveNode right(20, 1.0, 1.0, LINEAR);
	right.y = Keyframe(1.0);
	right.y.AddPoint(300, 0.75, LINEAR);

	curve.Nodes().push_back(left);
	curve.Nodes().push_back(right);

	CHECK(curve.Sample(0.0f, 1) == Approx(0.0f).margin(0.01f));
	CHECK(curve.Sample(0.0f, 300) == Approx(0.5f).margin(0.01f));
	CHECK(curve.Sample(1.0f, 1) == Approx(1.0f).margin(0.01f));
	CHECK(curve.Sample(1.0f, 300) == Approx(0.75f).margin(0.01f));

	const float midpoint = curve.Sample(0.5f, 150);
	CHECK(midpoint > 0.35f);
	CHECK(midpoint < 0.75f);
}

TEST_CASE("AnimatedCurve preserves bezier handles on evaluated runtime curve", "[animatedcurve][bezier]")
{
	AnimatedCurve curve;
	curve.Nodes().clear();

	AnimatedCurveNode left(1, 0.0, 0.0, BEZIER);
	left.right_handle_x = Keyframe(0.2);
	left.right_handle_y = Keyframe(0.0);

	AnimatedCurveNode middle(2, 0.5, 0.8, BEZIER);
	middle.left_handle_x = Keyframe(0.8);
	middle.left_handle_y = Keyframe(1.0);
	middle.right_handle_x = Keyframe(0.2);
	middle.right_handle_y = Keyframe(0.0);

	AnimatedCurveNode right(3, 1.0, 1.0, BEZIER);
	right.left_handle_x = Keyframe(0.8);
	right.left_handle_y = Keyframe(1.0);

	curve.Nodes().push_back(left);
	curve.Nodes().push_back(middle);
	curve.Nodes().push_back(right);

	const Keyframe runtime_curve = curve.BuildCurve(1, 255.0);
	CHECK(runtime_curve.GetCount() == 3);
	CHECK(runtime_curve.GetPoint(1).interpolation == BEZIER);
	CHECK(runtime_curve.GetPoint(1).handle_left.X == Approx(0.8f));
	CHECK(runtime_curve.GetPoint(1).handle_right.X == Approx(0.2f));
	CHECK(curve.Sample(0.5f, 1) == Approx(0.8f).margin(0.05f));
}

TEST_CASE("AnimatedCurve JSON round trip preserves animated nodes", "[animatedcurve][json]")
{
	AnimatedCurve curve;
	curve.enabled = Keyframe(1.0);
	curve.enabled.AddPoint(50, 0.0, LINEAR);
	curve.Nodes().clear();

	AnimatedCurveNode left(100, 0.0, 0.0, LINEAR);
	left.y.AddPoint(50, 0.25, LINEAR);
	AnimatedCurveNode right(200, 1.0, 1.0, LINEAR);
	right.x = Keyframe(1.0);
	right.x.AddPoint(50, 0.9, LINEAR);
	curve.Nodes().push_back(left);
	curve.Nodes().push_back(right);

	AnimatedCurve copy;
	copy.SetJson(curve.Json());

	CHECK(copy.Nodes().size() == 2);
	CHECK(copy.Nodes()[0].id == 100);
	CHECK(copy.Nodes()[0].y.GetValue(50) == Approx(0.25));
	CHECK(copy.Nodes()[1].x.GetValue(50) == Approx(0.9));
	CHECK(copy.enabled.GetValue(50) == Approx(0.0));
}
