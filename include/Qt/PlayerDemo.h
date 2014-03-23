/**
 *  @file player.h
 */

#ifndef OPENSHOT_PLAYER_H
#define OPENSHOT_PLAYER_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>
#include <QtGui/qevent.h>

namespace openshot
{
    class QtPlayer;
}

using openshot::QtPlayer;
class VideoRenderWidget;

class PlayerDemo : public QWidget
{
    Q_OBJECT

public:
    PlayerDemo(QWidget *parent = 0);
    ~PlayerDemo();

protected:
    void keyPressEvent(QKeyEvent *event);

private slots:
    void open(bool checked);

private:
    QVBoxLayout *vbox;
    QMenuBar *menu;
    VideoRenderWidget *video;
    QtPlayer *player;
};

#endif // OPENSHOT_PLAYER_H
