/**
 * @file
 * @brief Source file for PlayerBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PlayerBase.h"

using namespace openshot;

// Display a loading animation
void PlayerBase::Loading() {
	mode = PLAYBACK_LOADING;
}

// Play the video
void PlayerBase::Play() {
	mode = PLAYBACK_PLAY;
}

// Pause the video
void PlayerBase::Pause() {
	mode = PLAYBACK_PAUSED;
}

// Get the Playback speed
float PlayerBase::Speed() {
	return speed;
}

// Set the Playback speed multiplier (1.0 = normal speed, <1.0 = slower, >1.0 faster)
void PlayerBase::Speed(float new_speed) {
	speed = new_speed;
}

// Stop the video player and clear the cached frames
void PlayerBase::Stop() {
	mode = PLAYBACK_STOPPED;
}

// Get the current reader, such as a FFmpegReader
openshot::ReaderBase* PlayerBase::Reader() {
	return reader;
}

// Set the current reader, such as a FFmpegReader
void PlayerBase::Reader(openshot::ReaderBase *new_reader) {
	reader = new_reader;
}

// Get the Volume
float PlayerBase::Volume() {
	return volume;
}

// Set the Volume multiplier (1.0 = normal volume, <1.0 = quieter, >1.0 louder)
void PlayerBase::Volume(float new_volume) {
	volume = new_volume;
}
