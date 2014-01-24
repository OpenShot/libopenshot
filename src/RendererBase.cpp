/**
 * @file
 * @brief Source file for RendererBase class
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

#include "../include/RendererBase.h"
#include "../include/Frame.h"
#include <stdlib.h> // for realloc

using namespace openshot;

RendererBase::RendererBase() : buffer(NULL)
{
}

RendererBase::~RendererBase()
{
}

void RendererBase::paint(const std::tr1::shared_ptr<Frame> & frame)
{
    const tr1::shared_ptr<Magick::Image> image = frame->GetImage();
    const std::size_t width = image->columns();
    const std::size_t height = image->rows();
    const std::size_t bufferSize = width * height * 3;
    /// Use realloc for fast memory allocation.    
    /// TODO: consider locking buffer for mt safety
    buffer = reinterpret_cast<unsigned char*>(realloc(buffer, bufferSize));
    image->readPixels(Magick::RGBQuantum, buffer);
    this->render(RGB_888, width, height, width, buffer);
}
