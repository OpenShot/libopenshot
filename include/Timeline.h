#ifndef OPENSHOT_TIMELINE_H
#define OPENSHOT_TIMELINE_H

/**
 * \file
 * \brief Header file for Timeline class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include <list>
#include "Clip.h"
#include "FileReaderBase.h"
#include "Frame.h"
#include "FrameRate.h"
#include "KeyFrame.h"

using namespace std;
using namespace openshot;

namespace openshot {

	/// Comparison method for sorting clip pointers (by Position)
	struct compare_clip_pointers{
		bool operator()( Clip* lhs, Clip* rhs){
		return lhs->Position() < rhs->Position();
	}};

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
		int width; ///<Width of the canvas and viewport
		int height; ///<Height of the canvas and viewport
		Framerate fps; ///<Frames per second of the timeline
		list<Clip*> clips; ///<List of clips on this timeline
		map<Clip*, Clip*> open_clips; ///<List of 'opened' clips on this timeline

		/// Calculate time of a frame number, based on a framerate
		float calculate_time(int number, Framerate rate);

		/// Update the list of 'opened' clips
		void update_open_clips(Clip *clip, bool is_open);

	public:

		/// Default Constructor for the timeline (which sets the canvas width and height and FPS)
		Timeline(int width, int height, Framerate fps);

		/// Get an openshot::Frame object for a specific frame number of this timeline.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		Frame* GetFrame(int requested_frame) throw(ReaderClosed);

		/// Close the reader (and any resources it was consuming)
		void Close();

		/// Open the reader (and start consuming resources)
		void Open();

		/// Add an openshot::Clip to the timeline
		void AddClip(Clip* clip);

		/// Sort clips by position on the timeline
		void SortClips();

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
