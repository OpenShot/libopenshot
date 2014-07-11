/**
 * @file
 * @brief Source file for Video RendererWidget class
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

#include "../../include/Qt/VideoRenderWidget.h"
#include "../../include/Qt/VideoRenderer.h"
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>

VideoRenderWidget::VideoRenderWidget(QWidget *parent)
    : QWidget(parent)
    , renderer(new VideoRenderer(this))
{
    QPalette p = palette();
    p.setColor(QPalette::Window, Qt::black);
    setPalette(p);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    connect(renderer, SIGNAL(present(const QImage &)), this, SLOT(present(const QImage &)));
}

VideoRenderWidget::~VideoRenderWidget()
{
}

VideoRenderer *VideoRenderWidget::GetRenderer() const
{
    return renderer;
}

void VideoRenderWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    if (testAttribute(Qt::WA_OpaquePaintEvent)) {
        painter.fillRect(event->rect(), palette().window());
    }

    painter.drawImage(QRect(0, 0, width(), height()), image);
}

void VideoRenderWidget::present(const QImage & m)
{
    image = m;
    repaint();
}
