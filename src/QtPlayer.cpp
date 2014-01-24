/**
 * @file
 * @brief Source file for QtPlayer class
 * @author Duzy Chan <code@duzy.info>
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

QtPlayer::QtPlayer(RendererBase *rb) : PlayerBase(), p(new PlayerPrivate(rb))
{
    reader = NULL;
}

QtPlayer::~QtPlayer()
{
    if (mode != PLAYBACK_STOPPED) {
	//p->stop();
    }
    delete p;
}

void QtPlayer::SetSource(const std::string &source)
{
    reader = new FFmpegReader(source);
    reader->Open();
}

void QtPlayer::Play()
{
    mode = PLAYBACK_PLAY;
    p->position = 0;
    p->reader = reader;
    p->startPlayback();
}

void QtPlayer::Loading()
{
    mode = PLAYBACK_LOADING;
}

void QtPlayer::Pause()
{
    mode = PLAYBACK_PAUSED;
}

int QtPlayer::Position()
{
    return p->position;
}

void QtPlayer::Seek(int new_frame)
{
    p->position = new_frame;
}

void QtPlayer::Stop()
{
    mode = PLAYBACK_STOPPED;
}
