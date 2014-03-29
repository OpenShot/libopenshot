/**
 * @file
 * @brief Source file for RendererBase class
 * @author Duzy Chan <code@duzy.info>
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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
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
    const int BPP = 3;
    const tr1::shared_ptr<Magick::Image> image = frame->GetImage();
    const std::size_t width = image->columns();
    const std::size_t height = image->rows();
    const std::size_t bufferSize = width * height * BPP;
    /// Use realloc for fast memory allocation.    
    /// TODO: consider locking the buffer for mt safety
    buffer = reinterpret_cast<unsigned char*>(realloc(buffer, bufferSize));
#if false
    // Not sure if this is actually faster... but it works now
    image->getPixels(0,0, width, height); // load pixels into cache
    image->depth( 8 ); // this is required of it crashes
    image->writePixels(Magick::RGBQuantum, buffer); // write pixel data to our buffer
#else
    // Iterate through the pixel packets, and load our own buffer
    const Magick::PixelPacket *pixels = frame->GetPixels();
    for (int n = 0, i = 0; n < width * height; n += 1, i += 3) {
		buffer[i+0] = pixels[n].red   >> 8;
		buffer[i+1] = pixels[n].green >> 8;
		buffer[i+2] = pixels[n].blue  >> 8;
    }
#endif
    this->render(RGB_888, width, height, width * BPP, buffer);
}
