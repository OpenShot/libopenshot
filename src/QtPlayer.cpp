/**
 * @file
 * @brief Source file for QtPlayer class
 * @author Duzy Chan <code@duzy.info>
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

#include "../include/FFmpegReader.h"
#include "../include/QtPlayer.h"
#include "Qt/PlayerPrivate.h"

using namespace openshot;

QtPlayer::QtPlayer(RendererBase *rb) : PlayerBase(), p(new PlayerPrivate(rb)), threads_started(false)
{
	reader = NULL;
}

QtPlayer::~QtPlayer()
{
    if (mode != PLAYBACK_STOPPED) {
    	Stop();
    }
    delete p;
}

void QtPlayer::SetSource(const std::string &source)
{
    reader = new FFmpegReader(source);
    reader->Open();
	p->Reader(reader);
}

void QtPlayer::Play()
{
	cout << "PLAY() on QTPlayer" << endl;
	if (reader && !threads_started) {
		mode = PLAYBACK_PLAY;
		p->Reader(reader);
		p->startPlayback();
		threads_started = true;
	} else {
		mode = PLAYBACK_PLAY;
		Speed(1);
	}
}

void QtPlayer::Loading()
{
    mode = PLAYBACK_LOADING;
}

/// Get the current mode
PlaybackMode QtPlayer::Mode()
{
	return mode;
}

void QtPlayer::Pause()
{
    mode = PLAYBACK_PAUSED;
    Speed(0);
}

int QtPlayer::Position()
{
    return p->video_position;
}

void QtPlayer::Seek(int new_frame)
{
	// Seek the reader to a new position
    p->Seek(new_frame);
}

void QtPlayer::Stop()
{
    mode = PLAYBACK_STOPPED;
	p->stopPlayback();
	p->video_position = 0;
	threads_started = false;
}

// Set the reader object
void QtPlayer::Reader(ReaderBase *new_reader)
{
	reader = new_reader;
	p->Reader(new_reader);
}

// Get the current reader, such as a FFmpegReader
ReaderBase* QtPlayer::Reader() {
	return reader;
}

// Get the Playback speed
float QtPlayer::Speed() {
	return speed;
}

// Set the Playback speed multiplier (1.0 = normal speed, <1.0 = slower, >1.0 faster)
void QtPlayer::Speed(float new_speed) {
	speed = new_speed;
	p->Speed(new_speed);
}

// Get the Volume
float QtPlayer::Volume() {
	return volume;
}

// Set the Volume multiplier (1.0 = normal volume, <1.0 = quieter, >1.0 louder)
void QtPlayer::Volume(float new_volume) {
	volume = new_volume;
}
