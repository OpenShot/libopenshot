/**
 * @file
 * @brief Source file for PlayerPrivate class
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
#include "../include/ReaderBase.h"
#include "../include/RendererBase.h"
#include "PlayerPrivate.h"

namespace openshot
{

    PlayerPrivate::PlayerPrivate(RendererBase *rb)
	: position(0), renderer(rb)
    {
    }

    PlayerPrivate::~PlayerPrivate()
    {
	
    }

    void PlayerPrivate::startPlayback()
    {
	tr1::shared_ptr<Frame> frame = reader->GetFrame(position);
	if (!frame) return;
	renderer->paint(frame);
    }

}
