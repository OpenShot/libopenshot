#ifndef OPENSHOT_FRAMEMAPPER_H
#define OPENSHOT_FRAMEMAPPER_H

/**
 * \file
 * \brief Header file for the FrameMapper class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include <assert.h>
#include <iostream>
#include <math.h>
#include <vector>
#include "../include/FrameRate.h"
#include "../include/Exceptions.h"
#include "../include/KeyFrame.h"

using namespace std;

namespace openshot
{
	/**
	 * This enumeration determines how frame rates are increased or decreased, and
	 * whether to apply pull-down techniques or not.  Pull-down techniques are only
	 * needed to remove artificial fields added when converting between 24 fps (film)
	 * and television fps (29.97 fps NTSC or 25 fps PAL).
	 */
	enum Pulldown_Method
	{
		PULLDOWN_CLASSIC,	// Classic 2:3:2:3 pull-down
		PULLDOWN_ADVANCED,	// Advanced 2:3:3:2 pull-down (minimal dirty frames)
		PULLDOWN_NONE,		// Do not apply pull-down techniques, just repeat or skip entire frames
	};

	/**
	 * \brief This struct holds a single field (half a frame).
	 *
	 * A frame of video is made up of 2 fields (half a frame).  This struct points to which original
	 * frame, and whether this is the ODD or EVEN lines (i.e. top or bottom).
	 */
	struct Field
	{
		int Frame;
		bool isOdd;

		Field() : Frame(0), isOdd(true) { };

		Field(int frame, bool isodd)
		{
			Frame = frame;
			isOdd = isodd;
		}
	};

	/**
	 * \brief This struct holds two fields which together make up a complete video frame.
	 *
	 * These fields can point at different original frame numbers, for example the odd lines from
	 * frame 3, and the even lines of frame 4, if required by a pull-down technique.
	 */
	struct MappedFrame
	{
		Field Odd;
		Field Even;
	};

	/**
	 * \brief This class creates a mapping between 2 different frame rates, applying a specific pull-down technique.
	 *
	 * This class creates a mapping between 2 different video files, and supports many pull-down techniques,
	 * such as 2:3:2:3 or 2:3:3:2, and also supports inverse telecine. Pull-down techniques are only needed to remove
	 * artificial fields added when converting between 24 fps (film) and television fps (29.97 fps NTSC or 25 fps PAL).
	 *
	 * Please see the following <b>Example Code</b>:
	 * \code
	 * // Create a frame mapper for a clip with 100 frames, and convert the frame rate (from 24 fps to 29.97 fps)
	 * FrameMapper mapping(100, Framerate(24, 1), Framerate(30000, 1001), PULLDOWN_CLASSIC);
	 * Frame frame2 = mapping.GetFrame(2);
	 * assert(frame2.Odd.Frame == 2);
	 * \endcode
	 */
	class FrameMapper {
	private:
		int m_length;				// Length in frames of a video file
		vector<Field> fields;		// List of all fields
		vector<MappedFrame> frames;	// List of all frames
		bool field_toggle;			// Internal odd / even toggle (used when building the mapping)
		Framerate m_original;		// The original frame rate
		Framerate m_target;			// The target frame rate
		Pulldown_Method m_pulldown;	// The pull-down technique

		// Internal methods used by init
		void AddField(int frame);
		void AddField(Field field);

		// Use the original and target frame rates and a pull-down technique to create
		// a mapping between the original fields and frames or a video to a new frame rate.
		// This might repeat or skip fields and frames of the original video, depending on
		// whether the frame rate is increasing or decreasing.
		void Init();

	public:
		/// Default constructor for FrameMapper class
		FrameMapper(int Length, Framerate original, Framerate target, Pulldown_Method pulldown);

		/// Get a frame based on the target frame rate and the new frame number of a frame
		MappedFrame GetFrame(int TargetFrameNumber) throw(OutOfBoundsFrame);

		/// Get the target framerate
		Framerate TargetFPS() { return m_target; };

		/// Get the source framerate
		Framerate SourceFPS() { return m_original; };

		/// Set the target framerate
		void TargetFPS(Framerate new_fps) { m_target = new_fps; };

		/// Set the source framerate
		void SourceFPS(Framerate new_fps) { m_original = new_fps; };

		/**
		 * \brief Re-map time to slow down, speed up, or reverse a clip based on a Keyframe.
		 *
		 * This method re-maps the time of a clip, or in other words, changes the sequence and/or
		 * duration of frames in a clip.  Because this method accepts a Keyframe, the time, sequence,
		 * and direction of frames can be based on LINEAR, BEZIER, or CONSTANT values.
		 *
		 * The X axis of the Keyframe represents the time (in frames) of this clip.  If you make the
		 * X axis longer than the number of frames in the clip, if will slow down your clip.  If the
		 * X axis is shorter than your clip, it will speed up your clip.  The Y axis determines which
		 * original frame from the media file will be played.  For example, if you clip has 100 frames,
		 * and your Keyframe goes from (1,1) to (100,100), the clip will playback in the original sequence,
		 * and at the original speed.  If your Keyframe goes from (1,100) to (100,1), it will playback in the
		 * reverse direction, and at normal speed.  If your Keyframe goes from (1,100) to (500,1),
		 * it will play in reverse at 1/5 the original speed.
		 *
		 * Please see the following <b>Example Code</b>:
		 * \code
		 * // Create a mapping between 24 fps and 24 fps (a mapping is required for time-remapping)
		 * FrameMapper mapping(100, Framerate(24, 1), Framerate(24, 1), PULLDOWN_NONE);
		 *
		 * // Print the mapping (before it's been time-mapped)
		 * mapping.PrintMapping();
		 * cout << "-----------------------" << endl;
		 *
		 * // Create a Keyframe to re-map time (forward, reverse, and then forward again)
		 * Keyframe kf;
		 * kf.AddPoint(Point(Coordinate(1, 1), LINEAR));
		 * kf.AddPoint(Point(Coordinate(40, 40), LINEAR));
		 * kf.AddPoint(Point(Coordinate(60, 20), LINEAR)); // Reverse for 20 frames
		 * kf.AddPoint(Point(Coordinate(100, 100), LINEAR)); // Play to end (in fast forward)
		 *
		 * // Use the Keyframe to remap the time of this clip
		 * mapping.MapTime(kf);
		 *
		 * // Print the mapping again (to see the time remapping)
		 * mapping.PrintMapping();
		 * \endcode
		 */
		void MapTime(Keyframe new_time) throw(OutOfBoundsFrame);

		/// Print all of the original frames and which new frames they map to
		void PrintMapping();

	};
}

#endif
