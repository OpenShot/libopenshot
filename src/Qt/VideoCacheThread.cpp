/**
 * @file
 * @brief Source file for VideoCacheThread class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "VideoCacheThread.h"

#include "CacheBase.h"
#include "Exceptions.h"
#include "Frame.h"
#include "OpenMPUtilities.h"
#include "ZmqLogger.h"

#include <algorithm>
#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::chrono::microseconds

namespace openshot
{
	// Constructor
	VideoCacheThread::VideoCacheThread()
	: Thread("video-cache"), speed(1), is_playing(false),
	reader(NULL), max_frames_ahead(OPEN_MP_NUM_PROCESSORS * 2), current_display_frame(1)
    {
    }

    // Destructor
	VideoCacheThread::~VideoCacheThread()
    {
    }

	// Seek the reader to a particular frame number
	void VideoCacheThread::Seek(int64_t new_position)
	{
		current_display_frame = new_position;
	}

	// Play the video
	void VideoCacheThread::Play() {
		// Start playing
		is_playing = true;
	}

	// Stop the audio
	void VideoCacheThread::Stop() {
		// Stop playing
		is_playing = false;
	}

    // Start the thread
    void VideoCacheThread::run()
    {
      return;
    }

  int VideoCacheThread::MaxFramesAhead() {
      if (reader && reader->GetCache() && reader->GetCache()->GetMaxBytes() > 0) {
          int64_t bytes_per_frame = (reader->info.height * reader->info.width * 4) +
                                    (reader->info.sample_rate * reader->info.channels * 4);
          return (reader->GetCache()->GetMaxBytes() / bytes_per_frame) / 2;
      }
      return 0;
  }
}
