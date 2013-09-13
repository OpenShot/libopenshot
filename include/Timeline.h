/**
 * @file
 * @brief Header file for Timeline class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_TIMELINE_H
#define OPENSHOT_TIMELINE_H

// Defining some ImageMagic flags... for Mac (since they appear to be unset)
#ifndef MAGICKCORE_QUANTUM_DEPTH
	#define MAGICKCORE_QUANTUM_DEPTH 16
#endif
#ifndef MAGICKCORE_HDRI_ENABLE
	#define MAGICKCORE_HDRI_ENABLE 0
#endif

#include <list>
#include <omp.h>
#include <tr1/memory>
#include "Magick++.h"
#include "Cache.h"
#include "Color.h"
#include "Clip.h"
#include "ReaderBase.h"
#include "Fraction.h"
#include "Frame.h"
#include "FrameRate.h"
#include "KeyFrame.h"

using namespace std;
using namespace openshot;

namespace openshot {

	/// Comparison method for sorting clip pointers (by Position and Layer)
	struct CompareClips{
		bool operator()( Clip* lhs, Clip* rhs){
		return lhs->Position() <= rhs->Position() && lhs->Layer() < rhs->Layer();
	}};

	/**
	 * @brief This class represents a timeline
	 *
	 * The timeline is one of the most important features of a video editor, and controls all
	 * aspects of how video, image, and audio clips are combined together, and how the final
	 * video output will be rendered.  It has a collection of layers and clips, that arrange,
	 * sequence, and generate the final video output. Lets take a look at what the code looks like:
	 *
	 * @code
	 * // Create a Timeline
	 * Timeline t(1280, // width
	 *            720, // height
	 *            Framerate(25,1), // framerate
	 *            44100, // sample rate
	 *            2 // channels
	 *            );
	 *
	 * // Create some clips
	 * Clip c1(new ImageReader("MyAwesomeLogo.jpeg"));
	 * Clip c2(new FFmpegReader("BackgroundVideo.webm"));
	 *
	 * // CLIP 1 (logo) - Set some clip properties (with Keyframes)
	 * c1.Position(0.0);
	 * c1.gravity = GRAVITY_LEFT;
	 * c1.scale = SCALE_CROP;
	 * c1.Layer(1);
	 * c1.End(16.0);
	 * c1.alpha.AddPoint(1, 0.0);
	 * c1.alpha.AddPoint(500, 0.0);
	 * c1.alpha.AddPoint(565, 1.0);
	 *
	 * // CLIP 2 (background video) - Set some clip properties (with Keyframes)
	 * c2.Position(0.0);
	 * c2.Layer(0);
	 * c2.alpha.AddPoint(1, 1.0);
	 * c2.alpha.AddPoint(150, 0.0);
	 * c2.alpha.AddPoint(360, 0.0, LINEAR);
	 * c2.alpha.AddPoint(384, 1.0);
	 *
	 * // Add clips to timeline
	 * t.AddClip(&c1);
	 * t.AddClip(&c2);
	 *
	 * // Open the timeline reader
	 * t.Open();
	 *
	 * // Get frame number 1 from the timeline (This will generate a new frame, made up from the previous clips and settings)
	 * tr1::shared_ptr<Frame> f = t.GetFrame(1);
	 *
	 * // Now that we have an openshot::Frame object, lets have some fun!
	 * f->Display(); // Display the frame on the screen
	 *
	 * // Close the timeline reader
	 * t.Close();
	 *
	 * @endcode
	 */
	class Timeline : public ReaderBase {
	private:
		int width; ///<Width of the canvas and viewport
		int height; ///<Height of the canvas and viewport
		Framerate fps; ///<Frames per second of the timeline
		int sample_rate; ///<Sample rate of timeline
		int channels; ///<Channels in timeline
		list<Clip*> clips; ///<List of clips on this timeline
		list<Clip*> closing_clips; ///<List of clips that need to be closed
		map<Clip*, Clip*> open_clips; ///<List of 'opened' clips on this timeline
		Cache final_cache; ///<Final cache of timeline frames

		/// Process a new layer of video or audio
		void add_layer(tr1::shared_ptr<Frame> new_frame, Clip* source_clip, int clip_frame_number, int timeline_frame_number);

		/// Calculate time of a frame number, based on a framerate
		float calculate_time(int number, Framerate rate);

		/// Calculate the # of samples per video frame (for a specific frame number)
		int GetSamplesPerFrame(int frame_number);

		/// Compare 2 floating point numbers for equality
		bool isEqual(double a, double b);

		/// Update the list of 'opened' clips
		void update_open_clips(Clip *clip, bool is_open);

		/// Update the list of 'closed' clips
		void update_closed_clips();

	public:

		/// @brief Default Constructor for the timeline (which sets the canvas width and height and FPS)
		/// @param width The width of the timeline (and thus, the generated openshot::Frame objects)
		/// @param height The height of the timeline (and thus, the generated openshot::Frame objects)
		/// @param fps The frames rate of the timeline
		/// @param sample_rate The sample rate of the timeline's audio
		/// @param channels The number of audio channels of the timeline
		Timeline(int width, int height, Framerate fps, int sample_rate, int channels);

		/// @brief Add an openshot::Clip to the timeline
		/// @param clip Add an openshot::Clip to the timeline. A clip can contain any type of Reader.
		void AddClip(Clip* clip);

		/// @brief Remove an openshot::Clip to the timeline
		/// @param clip Remove an openshot::Clip from the timeline.
		void RemoveClip(Clip* clip);

		/// Close the reader (and any resources it was consuming)
		void Close();

		/// Get the framerate of this timeline
		Framerate FrameRate() { return fps; };

		/// @brief Set the framerate for this timeline
		/// @param new_fps The new frame rate to set for this timeline
		void FrameRate(Framerate new_fps) { fps = new_fps; };

		/// Get an openshot::Frame object for a specific frame number of this timeline.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed);

		/// Get the height of canvas and viewport
		int Height() { return height; }

		/// @brief Set the height of canvas and viewport
		/// @param new_height The new height to set for this timeline
		void Height(int new_height) { height = new_height; }

		// Curves for the viewport
		Keyframe viewport_scale; ///<Curve representing the scale of the viewport (0 to 100)
		Keyframe viewport_x; ///<Curve representing the x coordinate for the viewport
		Keyframe viewport_y; ///<Curve representing the y coordinate for the viewport

		// Background color
		Color color; ///<Background color of timeline canvas

		/// Open the reader (and start consuming resources)
		void Open();

		/// Sort clips by position on the timeline
		void SortClips();

		/// Get the width of canvas and viewport
		int Width() { return width; }

		/// @brief Set the width of canvas and viewport
		/// @param new_width The new width to set for this timeline (i.e. the canvas & viewport)
		void Width(int new_width) { width = new_width; }

	};


}

#endif
