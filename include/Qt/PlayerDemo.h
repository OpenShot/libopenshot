/**
 *  @file player.h
 */

#ifndef OPENSHOT_PLAYER_H
#define OPENSHOT_PLAYER_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>

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

private slots:
    void open(bool checked);

private:
    QVBoxLayout *vbox;
    QMenuBar *menu;
    VideoRenderWidget *video;
    QtPlayer *player;
};

#endif // OPENSHOT_PLAYER_H
