/**
 * @file
 * @brief Header file for Video RendererWidget class
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

#ifndef OPENSHOT_VIDEO_RENDERER_WIDGET_H
#define OPENSHOT_VIDEO_RENDERER_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtGui/QImage>
#include "../Fraction.h"
#include "VideoRenderer.h"


class VideoRenderWidget : public QWidget
{
    Q_OBJECT

private:
    VideoRenderer *renderer;
    QImage image;
    openshot::Fraction aspect_ratio;
    openshot::Fraction pixel_ratio;

public:
    VideoRenderWidget(QWidget *parent = 0);
    ~VideoRenderWidget();

    VideoRenderer *GetRenderer() const;
    void SetAspectRatio(openshot::Fraction new_aspect_ratio, openshot::Fraction new_pixel_ratio);

protected:
    void paintEvent(QPaintEvent *event);

    QRect centeredViewport(int width, int height);

private slots:
	void present(const QImage &image);

};

#endif // OPENSHOT_VIDEO_RENDERER_WIDGET_H
