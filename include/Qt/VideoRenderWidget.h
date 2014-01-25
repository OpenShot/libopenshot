/**
 *  @file VideoRenderWidget.h
 */

#ifndef __VIDEO_RENDER_WIDGET__
#define __VIDEO_RENDER_WIDGET__

#include <QtWidgets/QWidget>
#include <QImage>

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
    void present(const QImage & image);

private:
    VideoRenderer *renderer;
    QImage image;
};

#endif //__VIDEO_RENDER_WIDGET__
