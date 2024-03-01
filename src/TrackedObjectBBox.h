/**
 * @file
 * @brief Header file for the TrackedObjectBBox class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_TRACKEDOBJECTBBOX_H
#define OPENSHOT_TRACKEDOBJECTBBOX_H

#include "TrackedObjectBase.h"

#include "Color.h"
#include "Exceptions.h"
#include "Fraction.h"
#include "Json.h"
#include "KeyFrame.h"

namespace openshot
{
	/**
	 * @brief This struct holds the information of a bounding-box.
	 *
	 * A bounding-box is a rectangular shape that enclosures an
	 * object or a desired set of pixels in a digital image.
	 *
	 * The bounding-box structure holds five floating-point properties:
	 * the x and y coordinates of the rectangle's center point (cx, cy),
	 * the rectangle's width, height and rotation.
	 */
	struct BBox
	{
		float cx = -1; ///< x-coordinate of the bounding box center
		float cy = -1; ///< y-coordinate of the bounding box center
		float width = -1; ///< bounding box width
		float height = -1; ///< bounding box height
		float angle = -1; ///< bounding box rotation angle [degrees]

		/// Blank constructor
		BBox() {}

		/// Default constructor, which takes the bounding box top-left corner coordinates, width and height.
		/// @param _cx X-coordinate of the bounding box center
		/// @param _cy Y-coordinate of the bounding box center
		/// @param _width Bounding box width
		/// @param _height Bounding box height
		/// @param _angle Bounding box rotation angle [degrees]
		BBox(float _cx, float _cy, float _width, float _height, float _angle)
		{
			cx = _cx;
			cy = _cy;
			width = _width;
			height = _height;
			angle = _angle;
		}


		/// Generate JSON string of this object
		std::string Json() const
		{
			return JsonValue().toStyledString();
		}

		/// Generate Json::Value for this object
		Json::Value JsonValue() const
		{
			Json::Value root;
			root["cx"] = cx;
			root["cy"] = cy;
			root["width"] = width;
			root["height"] = height;
			root["angle"] = angle;

			return root;
		}

		/// Load JSON string into this object
		void SetJson(const std::string value)
		{
			// Parse JSON string into JSON objects
			try
			{
				const Json::Value root = openshot::stringToJson(value);
				// Set all values that match
				SetJsonValue(root);
			}
			catch (const std::exception &e)
			{
				// Error parsing JSON (or missing keys)
				throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
			}
		}

		/// Load Json::Value into this object
		void SetJsonValue(const Json::Value root)
		{

			// Set data from Json (if key is found)
			if (!root["cx"].isNull())
				cx = root["cx"].asDouble();
			if (!root["cy"].isNull())
				cy = root["cy"].asDouble();
			if (!root["width"].isNull())
				width = root["width"].asDouble();
			if (!root["height"].isNull())
				height = root["height"].asDouble();
			if (!root["angle"].isNull())
				angle = root["angle"].asDouble();
		}
	};

	/**
	 * @brief This class contains the properties of a tracked object
	 * and functions to manipulate it.
	 *
	 * The bounding-box displacement in X and Y directions, it's width,
	 * height and rotation variation over the frames are set as
	 * openshot::Keyframe objects.
	 *
	 * The bounding-box information over the clip's frames are
	 * saved into a protobuf file and loaded into an
	 * object of this class.
	 */
	class TrackedObjectBBox : public TrackedObjectBase
	{
	private:
		Fraction BaseFps;
		double TimeScale;

	public:
		std::map<double, BBox> BoxVec; ///< Index the bounding-box by time of each frame
		Keyframe delta_x; ///< X-direction displacement Keyframe
		Keyframe delta_y; ///< Y-direction displacement Keyframe
		Keyframe scale_x; ///< X-direction scale Keyframe
		Keyframe scale_y; ///< Y-direction scale Keyframe
		Keyframe rotation; ///< Rotation Keyframe
		Keyframe background_alpha; ///< Background box opacity
		Keyframe background_corner; ///< Radius of rounded corners
		Keyframe stroke_width; ///< Thickness of border line
		Keyframe stroke_alpha; ///< Stroke box opacity
		Color stroke; ///< Border line color
		Color background; ///< Background fill color

		std::string protobufDataPath; ///< Path to the protobuf file that holds the bounding box points across the frames

		/// Default Constructor
		TrackedObjectBBox();
		TrackedObjectBBox(int Red, int Green, int Blue, int Alfa);

		/// Add a BBox to the BoxVec map
		void AddBox(int64_t _frame_num, float _cx, float _cy, float _width, float _height, float _angle) override;

		/// Update object's BaseFps
		void SetBaseFPS(Fraction fps);

		/// Return the object's BaseFps
		Fraction GetBaseFPS();

		/// Update the TimeScale member variable
		void ScalePoints(double scale) override;

		/// Check if there is a bounding-box in the given frame
		bool Contains(int64_t frame_number) const;
		/// Check if there is a bounding-box in the exact frame number
		bool ExactlyContains(int64_t frame_number) const override;

		/// Get the size of BoxVec map
		int64_t GetLength() const;

		/// Remove a bounding-box from the BoxVec map
		void RemoveBox(int64_t frame_number);

		/// Return a bounding-box from BoxVec with it's properties adjusted by the Keyframes
		BBox GetBox(int64_t frame_number);
		/// Const-cast of the GetBox function, so that it can be called inside other cont function
		BBox GetBox(int64_t frame_number) const
		{
			return const_cast<TrackedObjectBBox *>(this)->GetBox(frame_number);
		}

		/// Load the bounding-boxes information from the protobuf file
		bool LoadBoxData(std::string inputFilePath);

		/// Get the time of the given frame
		double FrameNToTime(int64_t frame_number, double time_scale) const;

		/// Interpolate the bouding-boxes properties
		BBox InterpolateBoxes(double t1, double t2, BBox left, BBox right, double target);

		/// Clear the BoxVec map
		void clear();

		/// Get and Set JSON methods
		std::string Json() const override;				  ///< Generate JSON string of this object
		Json::Value JsonValue() const override;			 ///< Generate Json::Value for this object
		void SetJson(const std::string value) override;	 ///< Load JSON string into this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		Json::Value PropertiesJSON(int64_t requested_frame) const override;

		// Generate JSON for a property
		Json::Value add_property_json(std::string name, float value, std::string type, std::string memo, const Keyframe* keyframe, float min_value, float max_value, bool readonly, int64_t requested_frame) const;

		/// Return a map that contains the bounding box properties and it's keyframes indexed by their names
		std::map<std::string, float> GetBoxValues(int64_t frame_number) const override;
	};
} // namespace openshot

#endif
