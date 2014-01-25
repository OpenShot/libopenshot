/**
 *  @file VideoRenderWidget.cpp
 */

#include "../../include/Qt/VideoRenderer.h"
#include <QPainter>
#include <QImage>

using openshot::PixelFormat;

VideoRenderer::VideoRenderer(QObject *parent)
    : QObject(parent)
{
}

VideoRenderer::~VideoRenderer()
{
}

/*
void VideoRenderer::paint(QPainter *painter, const QRect &rect)
{
    // TODO: draw current video frame
}
*/

void VideoRenderer::render(PixelFormat /*format*/, int width, int height, int bytesPerLine, unsigned char *data)
{
    QImage image(data, width, height, bytesPerLine, QImage::Format_RGB888 /* TODO: render pixels */);
    emit present(image);
}
