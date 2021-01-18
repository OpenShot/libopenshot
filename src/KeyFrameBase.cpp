/**
 * @file
 * @brief Source file for the KeyframeBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "KeyFrameBase.h"
#include <algorithm>
#include <functional>
#include <utility>


namespace openshot{

	// Check if the X coordinate of a given Point is lower than a given value
	bool IsPointBeforeX(Point const & p, double const x) {
		return p.co.X < x;
	}

	// Linear interpolation between two points
	double InterpolateLinearCurve(Point const & left, Point const & right, double const target) {
		double const diff_Y = right.co.Y - left.co.Y;
		double const diff_X = right.co.X - left.co.X;
		double const slope = diff_Y / diff_X;
		return left.co.Y + slope * (target - left.co.X);
	}

	// Bezier interpolation between two points
	double InterpolateBezierCurve(Point const & left, Point const & right, double const target, double const allowed_error) {
		double const X_diff = right.co.X - left.co.X;
		double const Y_diff = right.co.Y - left.co.Y;
		Coordinate const p0 = left.co;
		Coordinate const p1 = Coordinate(p0.X + left.handle_right.X * X_diff, p0.Y + left.handle_right.Y * Y_diff);
		Coordinate const p2 = Coordinate(p0.X + right.handle_left.X * X_diff, p0.Y + right.handle_left.Y * Y_diff);
		Coordinate const p3 = right.co;

		double t = 0.5;
		double t_step = 0.25;
		do {
			// Bernstein polynoms
			double B[4] = {1, 3, 3, 1};
			double oneMinTExp = 1;
			double tExp = 1;
			for (int i = 0; i < 4; ++i, tExp *= t) {
				B[i] *= tExp;
			}
			for (int i = 0; i < 4; ++i, oneMinTExp *= 1 - t) {
				B[4 - i - 1] *= oneMinTExp;
			}
			double const x = p0.X * B[0] + p1.X * B[1] + p2.X * B[2] + p3.X * B[3];
			double const y = p0.Y * B[0] + p1.Y * B[1] + p2.Y * B[2] + p3.Y * B[3];
			if (fabs(target - x) < allowed_error) {
				return y;
			}
			if (x > target) {
				t -= t_step;
			}
			else {
				t += t_step;
			}
			t_step /= 2;
		} while (true);
	}

	// Interpolate two points using the right Point's interpolation method
	double InterpolateBetween(Point const & left, Point const & right, double target, double allowed_error) {
		assert(left.co.X < target);
		assert(target <= right.co.X);
		switch (right.interpolation) {
		case CONSTANT: return left.co.Y;
		case LINEAR: return InterpolateLinearCurve(left, right, target);
		case BEZIER: return InterpolateBezierCurve(left, right, target, allowed_error);
		}
	}

    KeyframeBase::KeyframeBase(){
		
    }
}