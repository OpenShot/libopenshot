/**
 *  @file VideoRenderWidget.cpp
 */

#include "../../include/Qt/VideoRenderer.h"
#include <QPainter>

VideoRenderer::VideoRenderer(QObject *parent)
    : QObject(parent)
{
}

VideoRenderer::~VideoRenderer()
{
}

void VideoRenderer::paint(QPainter *painter, const QRect &rect)
{
    // TODO: draw current video frame
}

void VideoRenderer::render(openshot::PixelFormat format, int width, int height, int bytesPerLine, unsigned char *data)
{
    // TODO: render pixels
}
