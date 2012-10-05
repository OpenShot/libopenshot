#ifndef OPENSHOT_TIMELINE_H
#define OPENSHOT_TIMELINE_H

/**
 * \file
 * \brief Header file for Timeline class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "FileReaderBase.h"
#include "Frame.h"
#include "KeyFrame.h"

using namespace std;
using namespace openshot;

namespace openshot {

	/**
	 * \brief This class represents a timeline
	 *
	 * The timeline is one of the most important features of a video editor, and controls all
	 * aspects of how video, image, and audio clips are combined together, and how the final
	 * video output will be rendered.  It has a collection of layers and clips, that arrange,
	 * sequence, and generate the final video output.
	 */
	class Timeline : public FileReaderBase {
	private:
		int width; ///<Width of the canvas
		int height; ///<Height of the canvas

	public:

		/// Default Constructor for the timeline (which sets the canvas width and height)
		Timeline(int width, int height);

		/// Get an openshot::Frame object for a specific frame number of this timeline.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		Frame* GetFrame(int requested_frame);

		/// Get the width of canvas and viewport
		int Width() { return width; }

		/// Get the height of canvas and viewport
		int Height() { return height; }

		/// Set the width of canvas and viewport
		void Width(int new_width) { width = new_width; }

		/// Set the height of canvas and viewport
		void Height(int new_height) { height = new_height; }

		// Curves for the viewport
		Keyframe viewport_scale; ///<Curve representing the scale of the viewport (0 to 100)
		Keyframe viewport_x; ///<Curve representing the x coordinate for the viewport
		Keyframe viewport_y; ///<Curve representing the y coordinate for the viewport
	};


}

#endif
