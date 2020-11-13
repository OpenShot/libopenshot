/**
 * @file
 * @brief Source file for the Keyframe class
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

#include "KeyFrameBBox.h"
#include <algorithm>
#include <functional>
//#include "Point.h"
//#include <utility>

using namespace std;
using namespace openshot;

namespace {
	bool IsPointBeforeX(Point const & p, double const x) {
		return p.co.X < x;
	}

	double InterpolateLinearCurve(Point const & left, Point const & right, double const target) {
		double const diff_Y = right.co.Y - left.co.Y;
		double const diff_X = right.co.X - left.co.X;
		double const slope = diff_Y / diff_X;
		return left.co.Y + slope * (target - left.co.X);
	}

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


	double InterpolateBetween(Point const & left, Point const & right, double target, double allowed_error) {
		assert(left.co.X < target);
		assert(target <= right.co.X);
		switch (right.interpolation) {
		case CONSTANT: return left.co.Y;
		case LINEAR: return InterpolateLinearCurve(left, right, target);
		case BEZIER: return InterpolateBezierCurve(left, right, target, allowed_error);
		}
	}


	template<typename Check>
	int64_t SearchBetweenPoints(Point const & left, Point const & right, int64_t const current, Check check) {
		int64_t start = left.co.X;
		int64_t stop = right.co.X;
		while (start < stop) {
			int64_t const mid = (start + stop + 1) / 2;
			double const value = InterpolateBetween(left, right, mid, 0.01);
			if (check(round(value), current)) {
				start = mid;
			} else {
				stop = mid - 1;
			}
		}
		return start;
	}
}


KeyFrameBBox::KeyFrameBBox(): delta_x(0.0), delta_y(0.0), scale_x(0.0), scale_y(0.0), rotation(0.0) {
    this->TimeScale = 1.0;
    return;
}

void KeyFrameBBox::AddDisplacement(int64_t frame_num, double _delta_x, double _delta_y){
    if (!this->Contains((int64_t) frame_num))
         return;

    double time = this->FrameNToTime(frame_num, 1.0);

    if (_delta_x != 0.0)
        delta_x.AddPoint(time, _delta_x, openshot::InterpolationType::LINEAR);
    if (_delta_y != 0.0)
        delta_y.AddPoint(time, _delta_y, openshot::InterpolationType::LINEAR);

    return;
}

void KeyFrameBBox::AddScale(int64_t frame_num, double _scale_x, double _scale_y){
    if (!this->Contains((double) frame_num))
         return;

    double time = this->FrameNToTime(frame_num, 1.0);

    if (_scale_x != 0.0)
        scale_x.AddPoint(time, _scale_x, openshot::InterpolationType::LINEAR);
    if (_scale_y != 0.0)
        scale_y.AddPoint(time, _scale_y, openshot::InterpolationType::LINEAR);
    
    return;
}

void KeyFrameBBox::AddRotation(int64_t _frame_num, double rot){
     if (!this->Contains((double) _frame_num))
         return;

    double time = this->FrameNToTime(_frame_num, 1.0);

    rotation.AddPoint(time, rot, openshot::InterpolationType::LINEAR);
}

void KeyFrameBBox::AddBox(int64_t _frame_num , float _cx, float _cy, float _width, float _height){
    
    if (_frame_num < 0)
        return;

    BBox box = BBox(_cx, _cy, _width, _height);

    double time = this->FrameNToTime(_frame_num, 1.0);

    auto it = BoxVec.find(time);
    if (it != BoxVec.end())
        it->second = box;
    else
        BoxVec.insert({time, box});
}

int64_t KeyFrameBBox::GetLength() const{
    
	if (BoxVec.empty()) 
        return 0;
	if (BoxVec.size() == 1) 
        return 1;

    return BoxVec.size();
}

bool KeyFrameBBox::Contains(int64_t frame_num) {
    
    double time = this->FrameNToTime(frame_num, 1.0);

    auto it = BoxVec.lower_bound(time);
    if (it == BoxVec.end())
        return false;
    
    return true;
}

void KeyFrameBBox::RemovePoint(int64_t frame_number){
    
    double time = this->FrameNToTime(frame_number, 1.0);

    auto it = BoxVec.find(time);
    if (it != BoxVec.end()){
        BoxVec.erase(time);
    
        RemoveDelta(time);
        RemoveScale(time);
    }
    return;
}

void KeyFrameBBox::RemoveDelta(int64_t frame_number) {

     double time = this->FrameNToTime(frame_number, 1.0);

    double attr_x = this->delta_x.GetValue(time);
    Point point_x = this->delta_x.GetClosestPoint(Point((double) time, attr_x));
    if (point_x.co.X == (double) time)
        this->delta_x.RemovePoint(point_x);
    
    double attr_y = this->delta_y.GetValue(time);
    Point point_y = this->delta_y.GetClosestPoint(Point((double) time, attr_y));
    if (point_y.co.X == (double) time)
        this->delta_y.RemovePoint(point_y);
    
    
    return;
}

void KeyFrameBBox::RemoveRotation(int64_t frame_number) {

     double time = this->FrameNToTime(frame_number, 1.0);

    double rot = this->rotation.GetValue(time);
    Point point_rot = this->rotation.GetClosestPoint(Point((double) time, rot));
    if (point_rot.co.X == (double) time)
        this->rotation.RemovePoint(point_rot);
    
    return;
}



void KeyFrameBBox::PrintParams() {
    std::cout << "delta_x ";
    this->delta_x.PrintPoints();
    
    std::cout << "delta_y ";
    this->delta_y.PrintPoints();
    
    std::cout << "scale_x ";
    this->scale_x.PrintPoints();
    
    std::cout << "scale_y ";
    this->scale_y.PrintPoints();

    std::cout << "rotation ";
    this->rotation.PrintPoints();
    
}


void KeyFrameBBox::RemoveScale(int64_t frame_number) {

    double time = this->FrameNToTime(frame_number, 1.0);

    double attr_x = this->scale_x.GetValue(time);
    Point point_x = this->scale_x.GetClosestPoint(Point((double) time, attr_x));
    if (point_x.co.X == (double) time)
        this->scale_x.RemovePoint(point_x);
    
    double attr_y = this->scale_y.GetValue(time);
    Point point_y = this->scale_y.GetClosestPoint(Point((double) time, attr_y));
    if (point_y.co.X == (double) time)
        this->scale_y.RemovePoint(point_y);
    
    
    return;
}

BBox KeyFrameBBox::GetValue(int64_t frame_number){
    double time = this->FrameNToTime(frame_number, this->TimeScale);

    auto it = BoxVec.lower_bound(time);
    if (it == BoxVec.end()){
        BBox resp;
        return resp;
    }
    
    if (it->first == time){
        BBox res = it->second;
        res.cx += this->delta_x.GetValue(time); 
        res.cy += this->delta_y.GetValue(time); 
        res.height += this->scale_y.GetValue(time); 
        res.width += this->scale_x.GetValue(time); 
        
        return res;
    }
    
    BBox second_ref = it->second;
    BBox first_ref = prev(it, 1)->second; 

    BBox res = InterpolateBoxes(prev(it, 1)->first, it->first, first_ref, second_ref, time);
        
    /*later add rotation transform to these points*/

    res.cx += this->delta_x.GetValue(time); 
    res.cy += this->delta_y.GetValue(time); 
    res.height += this->scale_y.GetValue(time); 
    res.width += this->scale_x.GetValue(time); 
        
    return res;
    

}

BBox KeyFrameBBox::InterpolateBoxes(double t1, double t2, BBox left, BBox right, double target){

    Point p1_left(t1, left.cx, openshot::InterpolationType::LINEAR);
    Point p1_right(t2, right.cx, openshot::InterpolationType::LINEAR);
    
    Point p1 = InterpolateBetween(p1_left, p1_right, target, 0.01);
    
    Point p2_left(t1, left.cy, openshot::InterpolationType::LINEAR);
    Point p2_right(t2, right.cy, openshot::InterpolationType::LINEAR);
    
    Point p2 = InterpolateBetween(p2_left, p2_right, target, 0.01);

    Point p3_left(t1, left.height, openshot::InterpolationType::LINEAR);
    Point p3_right(t2, right.height, openshot::InterpolationType::LINEAR);
    
    Point p3 = InterpolateBetween(p3_left, p3_right, target, 0.01);

    Point p4_left(t1, left.width, openshot::InterpolationType::LINEAR);
    Point p4_right(t2, right.width, openshot::InterpolationType::LINEAR);
    
    Point p4 = InterpolateBetween(p4_left, p4_right, target, 0.01);

    BBox ans(p1.co.Y, p2.co.Y, p3.co.Y, p4.co.Y);

    return ans;
}


void KeyFrameBBox::SetBaseFPS(Fraction fps){
    this->BaseFps = fps;
    return;
}

Fraction KeyFrameBBox::GetBaseFPS(){
    return BaseFps;
}

double KeyFrameBBox::FrameNToTime(int64_t frame_number, double time_scale){
    double time = ((double) frame_number) * this->BaseFps.Reciprocal().ToDouble() * time_scale;

    return time;
}

void KeyFrameBBox::ScalePoints(double time_scale){
    this->TimeScale = time_scale;
}


// Generate JSON string of this object
std::string KeyFrameBBox::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value KeyFrameBBox::JsonValue() {

	// Create root json object
	Json::Value root;
	//root["Points"] = Json::Value(Json::arrayValue);

    root["BaseFPS"]["num"] = BaseFps.num;
	root["BaseFPS"]["den"] = BaseFps.den;
    root["TimeScale"] = this->TimeScale;
    root["boxes"] = Json::Value(Json::arrayValue);

	// loop through points
    /*for (auto const& x : BoxVec){
        Json::Value elem;
        elem["key"] = x.first;
        elem["val"] = x.second.JsonValue();     
        root["boxes"].append(elem);
    }
    */

    root["delta_x"] = delta_x.JsonValue();
    root["delta_y"] = delta_y.JsonValue();
    root["scale_x"] = scale_x.JsonValue();
    root["scale_y"] = scale_y.JsonValue(); 
    root["rotation"] = rotation.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void KeyFrameBBox::SetJson(const std::string value) {
    
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
    
   return;
}

void KeyFrameBBox::clear(){
    BoxVec.clear();    
}



// Load Json::Value into this object
void KeyFrameBBox::SetJsonValue(const Json::Value root) {
	// Clear existing points
	BoxVec.clear();

    delta_x.SetJsonValue(root["delta_x"]);
    delta_y.SetJsonValue(root["delta_y"]);
    scale_x.SetJsonValue(root["scale_x"]);
    scale_y.SetJsonValue(root["scale_y"]);
    rotation.SetJsonValue(root["rotation"]);


	if (!root["BaseFPS"].isNull() && root["BaseFPS"].isObject()) {
        if (!root["BaseFPS"]["num"].isNull())
			BaseFps.num = (int) root["BaseFPS"]["num"].asInt();
        if (!root["BaseFPS"]["den"].isNull())
		    BaseFps.den = (int) root["BaseFPS"]["den"].asInt();
	}
    if (!root["TimeScale"].isNull()) {
        this->TimeScale = (double) root["TimeScale"].asDouble();
	}
    /*
	if (!root["boxes"].isNull()){
		// loop through points
		for (const auto existing_point : root["boxes"]) {
			// Create Point
            BBox box;
            box.SetJsonValue(existing_point["val"]);
            
            BoxVec.insert({existing_point["key"].asDouble(), box});
        }
    }
    */
    return;
}