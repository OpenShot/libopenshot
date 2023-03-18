/**
 * @file
 * @brief Header file for AudioLocation class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2023 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_AUDIOLOCATION_H
#define OPENSHOT_AUDIOLOCATION_H


namespace openshot
{
	/**
	 * @brief This struct holds the associated video frame and starting sample # for an audio packet.
	 *
	 * Because audio packets do not match up with video frames, this helps determine exactly
	 * where the audio packet's samples belong.
	 */
	struct AudioLocation {
		int64_t frame;
		int sample_start;

		bool is_near(AudioLocation location, int samples_per_frame, int64_t amount);
		AudioLocation() : frame(0), sample_start(0) {}
		AudioLocation(int64_t frame, int sample_start) : frame(frame), sample_start(sample_start) {}
	};
}

#endif
