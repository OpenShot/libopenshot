/**
 * @file
 * @brief Source file for PlayerBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/PlayerBase.h"

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
ReaderBase* PlayerBase::Reader() {
	return reader;
}

// Set the current reader, such as a FFmpegReader
void PlayerBase::Reader(ReaderBase *new_reader) {
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
