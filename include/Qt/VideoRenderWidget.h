/**
 *  @file VideoRenderWidget.h
 */

#ifndef __VIDEO_RENDER_WIDGET__
#define __VIDEO_RENDER_WIDGET__

#include <QtWidgets/QWidget>

class VideoRenderer;

class VideoRenderWidget : public QWidget
{
    Q_OBJECT

public:
    VideoRenderWidget(QWidget *parent = 0);
    ~VideoRenderWidget();

    VideoRenderer *GetRenderer() const;

protected:
    void paintEvent(QPaintEvent *event);

private slots:

private:
    VideoRenderer *renderer;
};

#endif //__VIDEO_RENDER_WIDGET__
