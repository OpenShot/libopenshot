/**
 * @file
 * @brief Header file for Caption effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CAPTION_EFFECT_H
#define OPENSHOT_CAPTION_EFFECT_H

#include <memory>
#include <string>
#include <vector>
#include <QFont>
#include <QFontMetrics>
#include <QRegularExpression>
#include "../Color.h"
#include "../EffectBase.h"
#include "../Json.h"
#include "../KeyFrame.h"



namespace openshot
{

/**
 * @brief This class adds captions/text over a video, based on timestamps. You can also animate some limited
 * aspects, such as words appearing/disappearing.
 *
 * Adding captions can be an easy way to generate text overlays through-out a long clip.
 */
class Caption : public EffectBase
{
private:
	std::vector<QRegularExpressionMatch> matchedCaptions; ///< RegEx to capture cues and text
	std::string caption_text;    ///< Text of caption
	QFontMetrics* metrics;       ///< Font metrics object
	QFont* font; 			     ///< QFont object
	bool is_dirty;

	/// Init effect settings
	void init_effect_details();

	/// Process regex capture
	void process_regex();


public:
	Color color;		 ///< Color of caption text
	Color stroke;		 ///< Color of text border / stroke
	Color background;	 ///< Color of caption area background
	Keyframe background_alpha;     ///< Background color alpha
	Keyframe background_corner;    ///< Background cornder radius
	Keyframe background_padding;    ///< Background padding
	Keyframe stroke_width;  ///< Width of text border / stroke
	Keyframe font_size;     ///< Font size in points
	Keyframe font_alpha;     ///< Font color alpha
	Keyframe line_spacing;	 ///< Distance between lines (1.0 default / 100%)
	Keyframe left;		 ///< Size of left bar
	Keyframe top;		 ///< Size of top bar
	Keyframe right;		 ///< Size of right bar
	Keyframe fade_in;		 ///< Fade in per caption (# of seconds)
	Keyframe fade_out;		 ///< Fade in per caption (# of seconds)
	std::string font_name;	///< Font string

	/// Blank constructor, useful when using Json to load the effect properties
	Caption();

	/// Default constructor, which takes a string of VTT/Subrip formatted caption data, and displays them over time.
	///
	/// @param captions A string with VTT/Subrip format text captions
	Caption(std::string captions);

	/// @brief This method is required for all derived classes of ClipBase, and returns a
	/// new openshot::Frame object. All Clip keyframes and effects are resolved into
	/// pixels.
	///
	/// @returns A new openshot::Frame object
	/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
	std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { return GetFrame(std::make_shared<openshot::Frame>(), frame_number); }

	/// @brief This method is required for all derived classes of ClipBase, and returns a
	/// modified openshot::Frame object
	///
	/// The frame object is passed into this method and used as a starting point (pixels and audio).
	/// All Clip keyframes and effects are resolved into pixels.
	///
	/// @returns The modified openshot::Frame object
	/// @param frame The frame object that needs the clip or effect applied to it
	/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
	std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

	// Get and Set caption data
	std::string CaptionText(); ///< Set the caption string to use (see VTT format)
	void CaptionText(std::string new_caption_text); ///< Get the caption string

	// Get and Set JSON methods
	std::string Json() const override; ///< Generate JSON string of this object
	void SetJson(const std::string value) override; ///< Load JSON string into this object
	Json::Value JsonValue() const override; ///< Generate Json::Value for this object
	void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

	/// Get all properties for a specific frame (perfect for a UI to display the current state
	/// of all properties at any time)
	std::string PropertiesJSON(int64_t requested_frame) const override;
};

}  // namespace openshot

#endif  // OPENSHOT_CAPTION_EFFECT_H
