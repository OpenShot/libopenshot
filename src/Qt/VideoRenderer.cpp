/**
 * @file
 * @brief Source file for VideoRenderer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#include "../../include/Qt/VideoRenderer.h"


VideoRenderer::VideoRenderer(QObject *parent)
    : QObject(parent)
{
}

VideoRenderer::~VideoRenderer()
{
}

/// Override QWidget which needs to be painted
void VideoRenderer::OverrideWidget(int64_t qwidget_address)
{
	// re-cast QWidget pointer (long) as an actual QWidget
	override_widget = reinterpret_cast<QWidget*>(qwidget_address);

}

void VideoRenderer::render(std::shared_ptr<QImage> image)
{
    if (image)
        emit present(*image);
}
