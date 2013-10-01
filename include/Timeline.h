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
#include "EffectBase.h"
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
		bool operator()( ClipBase* lhs, ClipBase* rhs){
		return lhs->Position() <= rhs->Position() && lhs->Layer() < rhs->Layer();
	}};

	/**
	 * @brief This class represents a timeline
	 *
	 * The timeline is one of the <b>most important</b> features of a video editor, and controls all
	 * aspects of how video, image, and audio clips are combined together, and how the final
	 * video output will be rendered.  It has a collection of layers and clips, that arrange,
	 * sequence, and generate the final video output.
	 *
	 * The <b>following graphic</b> displays a timeline, and how clips can be arranged, scaled, and layered together. It
	 * also demonstrates how the viewport can be scaled smaller than the canvas, which can be used to zoom and pan around the
	 * canvas (i.e. pan & scan).
	 * \image html /doc/images/Timeline_Layers.png
	 *
	 * The <b>following graphic</b> displays how the playhead determines which frames to combine and layer.
	 * \image html /doc/images/Playhead.png
	 *
	 * Lets take a look at what the code looks like:
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
	 * c1.Position(0.0); // Set the position or location (in seconds) on the timeline
	 * c1.gravity = GRAVITY_LEFT; // Set the alignment / gravity of the clip (position on the screen)
	 * c1.scale = SCALE_CROP; // Set the scale mode (how the image is resized to fill the screen)
	 * c1.Layer(1); // Set the layer of the timeline (higher layers cover up images of lower layers)
	 * c1.Start(0.0); // Set the starting position of the video (trim the left side of the video)
	 * c1.End(16.0); // Set the ending position of the video (trim the right side of the video)
	 * c1.alpha.AddPoint(1, 0.0); // Set the alpha to transparent on frame #1
	 * c1.alpha.AddPoint(500, 0.0); // Keep the alpha transparent until frame #500
	 * c1.alpha.AddPoint(565, 1.0); // Animate the alpha from transparent to visible (between frame #501 and #565)
	 *
	 * // CLIP 2 (background video) - Set some clip properties (with Keyframes)
	 * c2.Position(0.0); // Set the position or location (in seconds) on the timeline
	 * c2.Start(10.0); // Set the starting position of the video (trim the left side of the video)
	 * c2.Layer(0); // Set the layer of the timeline (higher layers cover up images of lower layers)
	 * c2.alpha.AddPoint(1, 1.0); // Set the alpha to visible on frame #1
	 * c2.alpha.AddPoint(150, 0.0); // Animate the alpha to transparent (between frame 2 and frame #150)
	 * c2.alpha.AddPoint(360, 0.0, LINEAR); // Keep the alpha transparent until frame #360
	 * c2.alpha.AddPoint(384, 1.0); // Animate the alpha to visible (between frame #360 and frame #384)
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
		list<EffectBase*> effects; ///<List of clips on this timeline
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

		/// @brief Add an effect to the timeline
		/// @param effect Add an effect to the timeline. An effect can modify the audio or video of an openshot::Frame.
		void AddEffect(EffectBase* effect);

		/// Return a list of clips on the timeline
		list<Clip*> Clips() { return clips; };

		/// Close the timeline reader (and any resources it was consuming)
		void Close();

		/// Return the list of effects on the timeline
		list<EffectBase*> Effects() { return effects; };

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

		/// @brief Remove an openshot::Clip from the timeline
		/// @param clip Remove an openshot::Clip from the timeline.
		void RemoveClip(Clip* clip);

		/// @brief Remove an effect from the timeline
		/// @param effect Remove an effect from the timeline.
		void RemoveEffect(EffectBase* effect);

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
