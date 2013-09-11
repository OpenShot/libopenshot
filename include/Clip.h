#ifndef OPENSHOT_CLIP_H
#define OPENSHOT_CLIP_H

/**
 * @file
 * @brief Header file for Clip class
 * @author Copyright (c) 2008-2013 OpenShot Studios, LLC
 */

/// Do not include the juce unittest headers, because it collides with unittest++
#ifndef __JUCE_UNITTEST_JUCEHEADER__
	#define __JUCE_UNITTEST_JUCEHEADER__
#endif

#include <tr1/memory>
#include "Color.h"
#include "FFmpegReader.h"
#include "FrameRate.h"
#include "FrameMapper.h"
#include "ImageReader.h"
#include "KeyFrame.h"
#include "JuceLibraryCode/JuceHeader.h"
#include "AudioResampler.h"

using namespace std;
using namespace openshot;

namespace openshot {

	/// This enumeration determines how clips are aligned to their parent container.
	enum GravityType
	{
		GRAVITY_TOP_LEFT,		///< Align clip to the top left of its parent
		GRAVITY_TOP,			///< Align clip to the top center of its parent
		GRAVITY_TOP_RIGHT,		///< Align clip to the top right of its parent
		GRAVITY_LEFT,			///< Align clip to the left of its parent (middle aligned)
		GRAVITY_CENTER,			///< Align clip to the center of its parent (middle aligned)
		GRAVITY_RIGHT,			///< Align clip to the right of its parent (middle aligned)
		GRAVITY_BOTTOM_LEFT,	///< Align clip to the bottom left of its parent
		GRAVITY_BOTTOM,			///< Align clip to the bottom center of its parent
		GRAVITY_BOTTOM_RIGHT	///< Align clip to the bottom right of its parent
	};

	/// This enumeration determines how clips are scaled to fit their parent container.
	enum ScaleType
	{
		SCALE_CROP,		///< Scale the clip until both height and width fill the canvas (cropping the overlap)
		SCALE_FIT,		///< Scale the clip until either height or width fills the canvas (with no cropping)
		SCALE_STRETCH,	///< Scale the clip until both height and width fill the canvas (distort to fit)
		SCALE_NONE		///< Do not scale the clip
	};

	/// This enumeration determines what parent a clip should be aligned to.
	enum AnchorType
	{
		ANCHOR_CANVAS,	///< Anchor the clip to the canvas
		ANCHOR_VIEWPORT	///< Anchor the clip to the viewport (which can be moved / animated around the canvas)
	};

	/**
	 * @brief This class represents a clip (used to arrange readers on the timeline)
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
		bool waveform; ///<Should a waveform be used instead of the clip's image

		// Audio resampler (if time mapping)
		AudioResampler *resampler;
		AudioSampleBuffer *audio_cache;

		// File Reader object
		ReaderBase* file_reader;

		/// Adjust frame number minimum value
		int adjust_frame_number_minimum(int frame_number);

		/// Get file extension
		string get_file_extension(string path);

		/// Adjust the audio and image of a time mapped frame
		tr1::shared_ptr<Frame> get_time_mapped_frame(tr1::shared_ptr<Frame> frame, int frame_number);

		/// Calculate the # of samples per video frame (for a specific frame number)
		int GetSamplesPerFrame(int frame_number, Fraction rate);

		/// Init default settings for a clip
		void init_settings();

		/// Reverse an audio buffer
		void reverse_buffer(juce::AudioSampleBuffer* buffer);

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

		/// @brief Constructor with filepath (reader is automatically created... by guessing file extensions)
		/// @param path The path of a reader (video file, image file, etc...). The correct reader will be used automatically.
		Clip(string path);

		/// @brief Constructor with reader
		/// @param reader The reader to be used by this clip
		Clip(ReaderBase* reader);

		/// Close the internal reader
		void Close();

		/// @brief Get an openshot::Frame object for a specific frame number of this timeline.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed);

		/// Open the internal reader
		void Open() throw(InvalidFile);

		/// @brief Set the current reader
		/// @param reader The reader to be used by this clip
		void Reader(ReaderBase* reader);

		/// Get the current reader
		ReaderBase* Reader();

		/// Get basic properties
		float Position() { return position; } ///<Get position on timeline
		int Layer() { return layer; } ///<Get layer of clip on timeline (lower number is covered by higher numbers)
		float Start() { return start; } ///<Get start position of clip (trim start of video)
		float End(); ///<Get end position of clip (trim end of video), which can be affected by the time curve.
		float Duration() { return End() - Start(); } ///<Get the length of this clip (in seconds)
		bool Waveform() { return waveform; } ///<Get the waveform property of this clip

		/// Set basic properties
		void Position(float value) { position = value; } ///<Get position on timeline
		void Layer(int value) { layer = value; } ///<Get layer of clip on timeline (lower number is covered by higher numbers)
		void Start(float value) { start = value; } ///<Get start position of clip (trim start of video)
		void End(float value) { end = value; } ///<Get end position of clip (trim end of video)
		void Waveform(bool value) { waveform = value; } ///<Set the waveform property of this clip

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
		Keyframe volume; ///<Curve representing the volume (0 to 1)

		// Audio waveform curves
		Color wave_color;

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
