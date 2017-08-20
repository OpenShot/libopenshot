/**
 * @file
 * @brief Header file for RendererBase class
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

#ifndef OPENSHOT_RENDERER_BASE_H
#define OPENSHOT_RENDERER_BASE_H

#include "../include/Frame.h"
#include <stdlib.h> // for realloc
#include <memory>

namespace openshot
{
    class Frame;

    /**
     * @brief This is the base class of all Renderers in libopenshot.
     *
     * Renderers are responsible for rendering images of a video onto a
     * display device.
     */
    class RendererBase
    {
    public:

	/// Paint(render) a video Frame.
	void paint(const std::shared_ptr<Frame> & frame);

	/// Allow manual override of the QWidget that is used to display
	virtual void OverrideWidget(long long qwidget_address) = 0;

    protected:
	RendererBase();
	virtual ~RendererBase();
	
	virtual void render(std::shared_ptr<QImage> image) = 0;
    };

}

#endif
