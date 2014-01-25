/**
 *  @file player.cpp
 */

#include "../../../include/QtPlayer.h"
#include "../../../include/Qt/PlayerDemo.h"
#include "../../../include/Qt/VideoRenderWidget.h"
#include "../../../include/Qt/VideoRenderer.h"
#include <QMessageBox>
#include <QFileDialog>

PlayerDemo::PlayerDemo(QWidget *parent)
    : QWidget(parent)
    , vbox(new QVBoxLayout(this))
    , menu(new QMenuBar(this))
    , video(new VideoRenderWidget(this))
    , player(new QtPlayer(video->GetRenderer()))
{
    setWindowTitle("Qt Player Demo");

    menu->setNativeMenuBar(true);

    QAction *action = NULL;
    action = menu->addAction("open");
    connect(action, SIGNAL(triggered(bool)), this, SLOT(open(bool)));

    vbox->addWidget(menu, 0);
    vbox->addWidget(video, 1);

    vbox->setMargin(0);
    vbox->setSpacing(0);
    resize(600, 480);
}

PlayerDemo::~PlayerDemo()
{
    delete player;
}

void PlayerDemo::open(bool checked)
{
    const QString filename = QFileDialog::getOpenFileName(this, "Open Video File");
    if (filename.isEmpty()) return;
    player->SetSource(filename.toStdString());
    player->Play();
}
