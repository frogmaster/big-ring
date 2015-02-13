/*
 * Copyright (c) 2012-2015 Ilja Booij (ibooij@gmail.com)
 *
 * This file is part of Big Ring Indoor Video Cycling
 *
 * Big Ring Indoor Video Cycling is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Big Ring Indoor Video Cycling  is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with Big Ring Indoor Video Cycling.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "newvideowidget.h"

#include <functional>

#include <QtCore/QtDebug>
#include <QtCore/QUrl>
#include <QtOpenGL/QGLWidget>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtGui/QResizeEvent>

#include "clockgraphicsitem.h"
#include "profileitem.h"
#include "sensoritem.h"
#include "simulation.h"
#include "screensaverblocker.h"
#include "videoplayer.h"


NewVideoWidget::NewVideoWidget( Simulation& simulation, QWidget *parent) :
    QGraphicsView(parent), _screenSaverBlocker(new indoorcycling::ScreenSaverBlocker(this, this)), _mouseIdleTimer(new QTimer(this))
{
    setMinimumSize(800, 600);
    setFocusPolicy(Qt::StrongFocus);
    QGLWidget* viewPortWidget = new QGLWidget(QGLFormat(QGL::SampleBuffers));
    setViewport(viewPortWidget);
    setFrameShape(QFrame::NoFrame);

    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QGraphicsScene* scene = new QGraphicsScene(this);
    setScene(scene);

    setSizeAdjustPolicy(QGraphicsView::AdjustIgnored);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setCacheMode(QGraphicsView::CacheNone);

    addClock(simulation, scene);
    addSensorItems(simulation, scene);

    _profileItem = new ProfileItem(&simulation);
    scene->addItem(_profileItem);

    setupVideoPlayer(viewPortWidget);

    _pausedItem = new QGraphicsTextItem;
    QFont bigFont;
    bigFont.setPointSize(36);
    _pausedItem->setFont(bigFont);
    _pausedItem->setDefaultTextColor(Qt::white);
    _pausedItem->setPlainText("Paused");

    connect(&simulation, &Simulation::playing, this, [=](bool playing) {
        if (playing) {
            _pausedItem->hide();
        } else {
            _pausedItem->show();
        }
    });
    scene->addItem(_pausedItem);

    _mouseIdleTimer->setInterval(500);
    _mouseIdleTimer->setSingleShot(true);
    connect(_mouseIdleTimer, &QTimer::timeout, _mouseIdleTimer, []() {
        QApplication::setOverrideCursor(Qt::BlankCursor);
    });
}

void NewVideoWidget::setupVideoPlayer(QGLWidget* paintWidget)
{
    _videoPlayer = new VideoPlayer(paintWidget, this);

    connect(_videoPlayer, &VideoPlayer::videoLoaded, this, [this](qint64 nanoSeconds) {
        qDebug() << "duration" << nanoSeconds;
        this->_rlv.setDuration(nanoSeconds / 1000);
        if (this->_course.isValid()) {
            this->seekToStart(_course);
        }
    });
    connect(_videoPlayer, &VideoPlayer::seekDone, this, [this]() {
        emit readyToPlay(true);
    });
    connect(_videoPlayer, &VideoPlayer::updateVideo, this, [this]() {
        this->viewport()->update();
    });
}

void NewVideoWidget::addClock(Simulation &simulation, QGraphicsScene* scene)
{
    _clockItem = new ClockGraphicsItem;
    connect(&simulation, &Simulation::runTimeChanged, _clockItem, &ClockGraphicsItem::setTime);
    scene->addItem(_clockItem);
}

void NewVideoWidget::addHeartRate(Simulation &simulation, QGraphicsScene *scene)
{
    SensorItem* heartRateItem = new SensorItem(QuantityPrinter::HeartRate);
    scene->addItem(heartRateItem);
    connect(&simulation.cyclist(), &Cyclist::heartRateChanged, this, [heartRateItem](quint8 heartRate) {
        heartRateItem->setValue(QVariant::fromValue(static_cast<int>(heartRate)));
    });
    _heartRateItem = heartRateItem;
}

NewVideoWidget::~NewVideoWidget()
{

}

bool NewVideoWidget::isReadyToPlay()
{
    return _videoPlayer->isReadyToPlay();
}

void NewVideoWidget::setRealLifeVideo(RealLifeVideo rlv)
{
    Q_EMIT(readyToPlay(false));
    _rlv = rlv;
    _profileItem->setRlv(rlv);
    _videoPlayer->stop();

    QString uri = rlv.videoInformation().videoFilename();
    if (uri.indexOf("://") < 0) {
        uri = QUrl::fromLocalFile(uri).toEncoded();
    }
    _videoPlayer->loadVideo(uri);
}

void NewVideoWidget::setCourse(Course &course)
{
    _course = course;
    seekToStart(_course);
}

void NewVideoWidget::setCourseIndex(int index)
{
    if (!_rlv.isValid()) {
        return;
    }
    Course course = _rlv.courses()[qMax(0, index)];
    setCourse(course);
}

void NewVideoWidget::setDistance(float distance)
{
    _videoPlayer->stepToFrame(_rlv.frameForDistance(distance));
}

void NewVideoWidget::goToFullscreen()
{
    showFullScreen();
}

void NewVideoWidget::resizeEvent(QResizeEvent *resizeEvent)
{
    setSceneRect(viewport()->rect());
    QRectF clockItemRect = _clockItem->boundingRect();
    clockItemRect.moveCenter(QPointF(sceneRect().width() / 2, 0.0));
    clockItemRect.moveTop(0.0);
    _clockItem->setPos(clockItemRect.topLeft());


    QPointF scenePosition = mapToScene(width() / 2, 0);
    scenePosition = mapToScene(0, height() /8);
    _wattageItem->setPos(scenePosition);
    scenePosition = mapToScene(0, 2* height() /8);
    _heartRateItem->setPos(scenePosition);
    scenePosition = mapToScene(0, 3 * height() /8);
    _cadenceItem->setPos(scenePosition);
    scenePosition = mapToScene(width(), 1 * height() / 8);
    _speedItem->setPos(scenePosition.x() - _speedItem->boundingRect().width(), scenePosition.y());
    scenePosition = mapToScene(width(), 2 * height() /8);
    _distanceItem->setPos(scenePosition.x() - _distanceItem->boundingRect().width(), scenePosition.y());
    scenePosition = mapToScene(width(), 3 * height() / 8);
    _gradeItem->setPos(scenePosition.x() - _gradeItem->boundingRect().width(), scenePosition.y());
    resizeEvent->accept();

    QPointF center = mapToScene(viewport()->rect().center());
    _pausedItem->setPos(center);

    _profileItem->setGeometry(QRectF(mapToScene(resizeEvent->size().width() * 1 / 16, resizeEvent->size().height() * 27 / 32), QSizeF(sceneRect().width() * 7 / 8, sceneRect().height() * 1 / 8)));

}

void NewVideoWidget::enterEvent(QEvent *)
{
    QApplication::setOverrideCursor(Qt::BlankCursor);
    _mouseIdleTimer->start();
}

void NewVideoWidget::leaveEvent(QEvent *)
{
    QApplication::setOverrideCursor(Qt::ArrowCursor);
    _mouseIdleTimer->stop();
}

void NewVideoWidget::mouseMoveEvent(QMouseEvent *)
{
    QApplication::setOverrideCursor(Qt::ArrowCursor);
    _mouseIdleTimer->stop();
    _mouseIdleTimer->start();
}

void NewVideoWidget::closeEvent(QCloseEvent *)
{
    QApplication::setOverrideCursor(Qt::ArrowCursor);
}

/*!
 *
 * \brief Draw background by telling the video sink to update the complete background.
 * \param painter the painter used for drawing.
 */
void NewVideoWidget::drawBackground(QPainter *painter, const QRectF &)
{
    QPointF topLeft = mapToScene(viewport()->rect().topLeft());
    QPointF bottemRight = mapToScene(viewport()->rect().bottomRight());
    const QRectF r = QRectF(topLeft, bottemRight);
    _videoPlayer->displayCurrentFrame(painter, r, Qt::KeepAspectRatioByExpanding);

}

void NewVideoWidget::seekToStart(Course &course)
{
    quint32 frame = _rlv.frameForDistance(course.start());
    _videoPlayer->seekToFrame(frame, _rlv.videoInformation().frameRate());
}

void NewVideoWidget::addSensorItems(Simulation &simulation, QGraphicsScene *scene)
{
    addWattage(simulation, scene);
    addHeartRate(simulation, scene);
    addCadence(simulation, scene);
    addSpeed(simulation, scene);
    addDistance(simulation, scene);
    addGrade(simulation, scene);
}

void NewVideoWidget::addWattage(Simulation &simulation, QGraphicsScene *scene)
{
    SensorItem* wattageItem = new SensorItem(QuantityPrinter::Power);
    scene->addItem(wattageItem);
    connect(&simulation.cyclist(), &Cyclist::powerChanged, this, [wattageItem](float power) {
        wattageItem->setValue(QVariant::fromValue(power));
    });
    _wattageItem = wattageItem;
}

void NewVideoWidget::addCadence(Simulation &simulation, QGraphicsScene *scene)
{
    SensorItem* cadenceItem = new SensorItem(QuantityPrinter::Cadence);
    scene->addItem(cadenceItem);
    connect(&simulation.cyclist(), &Cyclist::cadenceChanged, this, [cadenceItem](quint8 cadence) {
        cadenceItem->setValue(QVariant::fromValue(static_cast<int>(cadence)));
    });
    _cadenceItem = cadenceItem;
}

void NewVideoWidget::addSpeed(Simulation &simulation, QGraphicsScene *scene)
{
    SensorItem* speedItem = new SensorItem(QuantityPrinter::Speed);
    scene->addItem(speedItem);
    connect(&simulation.cyclist(), &Cyclist::speedChanged, this, [speedItem](float speed) {
        speedItem->setValue(QVariant::fromValue(speed));
    });
    _speedItem = speedItem;
}

void NewVideoWidget::addGrade(Simulation &simulation, QGraphicsScene *scene)
{
    SensorItem* gradeItem = new SensorItem(QuantityPrinter::Grade);
    scene->addItem(gradeItem);
    connect(&simulation, &Simulation::slopeChanged, this, [gradeItem](float grade) {
        gradeItem->setValue(QVariant::fromValue(grade));
    });
    _gradeItem = gradeItem;
}

void NewVideoWidget::addDistance(Simulation &simulation, QGraphicsScene *scene)
{
    SensorItem* distanceItem = new SensorItem(QuantityPrinter::Distance);
    scene->addItem(distanceItem);
    connect(&simulation.cyclist(), &Cyclist::distanceTravelledChanged, this, [distanceItem](float distance) {
        distanceItem->setValue(QVariant::fromValue(static_cast<int>(distance)));
    });
    _distanceItem = distanceItem;
}
