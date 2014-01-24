/**
 *  @file VideoRenderer.h
 */

#ifndef __VIDEO_RENDERER__
#define __VIDEO_RENDERER__

#include "../RendererBase.h"
#include <QtCore/QObject>
#include <tr1/memory>

class QPainter;

class VideoRenderer : public QObject, public openshot::RendererBase
{
    Q_OBJECT

public:
    VideoRenderer(QObject *parent = 0);
    ~VideoRenderer();

    void paint(QPainter *painter, const QRect &rect);

protected:
    void render(openshot::PixelFormat format, int width, int height, int bytesPerLine, unsigned char *data);

private slots:

private:
};

#endif //__VIDEO_RENDERER__
