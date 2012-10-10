#ifndef OPENSHOT_CLIP_H
#define OPENSHOT_CLIP_H

/**
 * \file
 * \brief Header file for Clip class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "FFmpegReader.h"
#include "FrameRate.h"
#include "FrameMapper.h"
#include "ImageReader.h"
#include "KeyFrame.h"

using namespace std;
using namespace openshot;

namespace openshot {

	/**
	 * This enumeration determines how clips are aligned to their parent container.
	 */
	enum GravityType
	{
		GRAVITY_TOP_LEFT,
		GRAVITY_TOP,
		GRAVITY_TOP_RIGHT,
		GRAVITY_LEFT,
		GRAVITY_CENTER,
		GRAVITY_RIGHT,
		GRAVITY_BOTTOM_LEFT,
		GRAVITY_BOTTOM,
		GRAVITY_BOTTOM_RIGHT
	};

	/**
	 * This enumeration determines how clips are scaled to fit their parent container.
	 */
	enum ScaleType
	{
		SCALE_CROP,
		SCALE_FIT,
		SCALE_STRETCH,
		SCALE_NONE
	};

	/**
	 * This enumeration determines what parent a clip should be aligned to.
	 */
	enum AnchorType
	{
		ANCHOR_CANVAS,
		ANCHOR_VIEWPORT
	};

	/**
	 * \brief This class represents a clip
	 *
	 * Each image, video, or audio file is represented on a layer as a clip.  A clip has many
	 * properties that affect how it behaves on the timeline, such as its size, position,
	 * transparency, rotation, speed, volume, etc...
	 */
	class Clip {
	private:
		float position; ///<The position of the timeline where this clip should start playing
		int layer; ///<The layer this clip is on. Lower clips are covered up by higher clips.
		float start; ///<The position in seconds to start playing (used to trim the beginning of a clip)
		float end; ///<The position in seconds to end playing (used to trim the ending of a clip)

		// File Reader object
		FileReaderBase* file_reader;

		// Internal frame mapper (to map between different frame rates)
		FrameMapper* frame_map;

		/// Adjust frame number minimum value
		int adjust_frame_number_minimum(int frame_number);

		/// Apply basic image processing (scale, rotate, move, etc...)
		void apply_basic_image_processing(Frame* frame, int frame_number);

		/// Get file extension
		string get_file_extension(string path);

		/// Get the new frame number, based on the Framemapper, or return the number passed in
		int get_framerate_mapped_frame(int original_frame_number);

		/// Get the new frame number, based on a time map curve (used to increase speed, change direction, etc...)
		int get_time_mapped_frame(int original_frame_number);

		/// Init default settings for a clip
		void init_settings();

	public:
		GravityType gravity; ///<The gravity of a clip determines where it snaps to it's parent
		ScaleType scale; ///<The scale determines how a clip should be resized to fit it's parent
		AnchorType anchor; ///<The anchor determines what parent a clip should snap to

		// Compare a clip using the Position() property
		bool operator< ( Clip& a) { return (Position() < a.Position()); }
		bool operator<= ( Clip& a) { return (Position() <= a.Position()); }
		bool operator> ( Clip& a) { return (Position() > a.Position()); }
		bool operator>= ( Clip& a) { return (Position() >= a.Position()); }

		/// Default Constructor
		Clip();

		/// Constructor with filepath (reader is automatically created... by guessing file extensions)
		Clip(string path);

		/// Constructor with reader
		Clip(FileReaderBase* reader);

		/// Close the internal reader
		void Close();

		/// Get an openshot::Frame object for a specific frame number of this timeline.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		Frame* GetFrame(int requested_frame) throw(ReaderClosed);

		/// Map frame rate of this clip to a different frame rate
		void MapFrames(Framerate fps, Pulldown_Method pulldown);

		/// Open the internal reader
		void Open() throw(InvalidFile);

		/// Set the current reader
		void Reader(FileReaderBase* reader);

		/// Get the current reader
		FileReaderBase* Reader();

		/// Get basic properties
		float Position() { return position; } ///<Get position on timeline
		int Layer() { return layer; } ///<Get layer of clip on timeline (lower number is covered by higher numbers)
		float Start() { return start; } ///<Get start position of clip (trim start of video)
		float End() { return end; } ///<Get end position of clip (trim end of video)

		/// Set basic properties
		void Position(float value) { position = value; } ///<Get position on timeline
		void Layer(int value) { layer = value; } ///<Get layer of clip on timeline (lower number is covered by higher numbers)
		void Start(float value) { start = value; } ///<Get start position of clip (trim start of video)
		void End(float value) { end = value; } ///<Get end position of clip (trim end of video)

		// Scale and Location curves
		Keyframe scale_x; ///<Curve representing the horizontal scaling in percent (0 to 100)
		Keyframe scale_y; ///<Curve representing the vertical scaling in percent (0 to 100)
		Keyframe location_x; ///<Curve representing the relative X position in percent based on the gravity (-100 to 100)
		Keyframe location_y; ///<Curve representing the relative Y position in percent based on the gravity (-100 to 100)

		// Alpha and Rotation curves
		Keyframe alpha; ///<Curve representing the alpha or transparency (0 to 100)
		Keyframe rotation; ///<Curve representing the rotation (0 to 360)

		// Time and Volume curves
		Keyframe time; ///<Curve representing the frames over time to play (used for speed and direction of video)
		Keyframe volume; ///<Curve representing the volume (0 to 100)

		// Crop settings and curves
		GravityType crop_gravity; ///<Cropping needs to have a gravity to determine what side we are cropping
		Keyframe crop_width; ///<Curve representing width in percent (0.0=0%, 1.0=100%)
		Keyframe crop_height; ///<Curve representing height in percent (0.0=0%, 1.0=100%)
		Keyframe crop_x; ///<Curve representing X offset in percent (-1.0=-100%, 0.0=0%, 1.0=100%)
		Keyframe crop_y; ///<Curve representing Y offset in percent (-1.0=-100%, 0.0=0%, 1.0=100%)

		// Shear and perspective curves
		Keyframe shear_x; ///<Curve representing X shear angle in degrees (-45.0=left, 45.0=right)
		Keyframe shear_y; ///<Curve representing Y shear angle in degrees (-45.0=down, 45.0=up)
		Keyframe perspective_c1_x; ///<Curves representing X for coordinate 1
		Keyframe perspective_c1_y; ///<Curves representing Y for coordinate 1
		Keyframe perspective_c2_x; ///<Curves representing X for coordinate 2
		Keyframe perspective_c2_y; ///<Curves representing Y for coordinate 2
		Keyframe perspective_c3_x; ///<Curves representing X for coordinate 3
		Keyframe perspective_c3_y; ///<Curves representing Y for coordinate 3
		Keyframe perspective_c4_x; ///<Curves representing X for coordinate 4
		Keyframe perspective_c4_y; ///<Curves representing Y for coordinate 4

	};


}

#endif
