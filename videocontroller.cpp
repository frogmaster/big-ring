#include "videocontroller.h"

#include <QtDebug>
#include <QDateTime>
#include <QTimer>

namespace {
const float SPEED = 30.0f / 3.6f;
const float videoUpdateInterval = 100; // ms
}

VideoController::VideoController(VideoWidget* videoWidget, QObject *parent) :
	QObject(parent),
	_imageQueue(100, 50),
	_videoDecoder(&_imageQueue),
	_videoWidget(videoWidget),
	_currentDistance(0.0f),
	_running(false)
{
	_decoderThread.start();
	_videoDecoder.moveToThread(&_decoderThread);

	// set up timers
	_playTimer.setInterval(40);
	connect(&_playTimer, SIGNAL(timeout()), SLOT(displayFrame()));
	_updateTimer.setInterval(videoUpdateInterval);
	connect(&_updateTimer, SIGNAL(timeout()), SLOT(updateVideo()));

	// set up video decoder
	connect(&_videoDecoder, SIGNAL(videoLoaded()), SLOT(videoLoaded()));
}

VideoController::~VideoController()
{
	_decoderThread.quit();
	_decoderThread.wait(1000); // wait for a maximum of 1 second.
}

void VideoController::realLiveVideoSelected(RealLiveVideo rlv)
{
	reset();
	_currentRlv = rlv;
	loadVideo(_currentRlv.videoInformation().videoFilename());
	setDistance(0.0f);
	_lastTime = QDateTime::currentMSecsSinceEpoch();
}

void VideoController::courseSelected(int courseNr)
{
	reset();

	if (courseNr == -1) {
		return;
	}

	if (!_currentRlv.isValid())
		return;

	const Course& course = _currentRlv.courses()[courseNr];
	setDistance(course.start());
	_lastTime = QDateTime::currentMSecsSinceEpoch();
	quint32 frame = _currentRlv.frameForDistance(_currentDistance);
	qDebug() << "slope at start = " << _currentRlv.slopeForDistance(_currentDistance);

	setPosition(frame);
}

void VideoController::play(bool doPlay)
{
	if (doPlay) {
		_playTimer.start();
		_updateTimer.start();
	} else {
		reset();
	}
}


void VideoController::updateVideo()
{
//	float metersPerFrame = _currentRlv.metersPerFrame(_currentDistance);
	float slope = _currentRlv.slopeForDistance(_currentDistance);
	emit slopeChanged(slope);
	emit altitudeChanged(_currentRlv.altitudeForDistance(_currentDistance));

	// speed is 30 km/h -> 8.3333 m/s
//	float framesPerSecond = SPEED / metersPerFrame;

}

void VideoController::displayFrame()
{
	updateDistance();
	quint32 frameToShow = _currentRlv.frameForDistance(_currentDistance);
	if (_currentFrame.image().isNull()) {
		qDebug() << "current frame is null, taking first one";
		_currentFrame = _imageQueue.take();
		_videoWidget->displayFrame(_currentFrame);
	}
	if (frameToShow == _currentFrame.frameNr())
		return; // no need to display again.
	if (frameToShow < _currentFrame.frameNr())
		return; // wait until playing catches up.

	bool displayed = false;
	while(!displayed && !_currentFrame.image().isNull()) {
		_currentFrame = _imageQueue.take();
		if (_currentFrame.frameNr() == frameToShow) {
			_videoWidget->displayFrame(_currentFrame);
			displayed = true;
		}
	}
}

void VideoController::updateDistance()
{
	if (!_currentRlv.isValid())
		return;

	// update distance
	qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
	qint64 elapsed = currentTime - _lastTime;
	_lastTime = currentTime;

	float distanceTravelled = (SPEED * elapsed) * 0.001;

//	qDebug() << elapsed << "  distance travelled = " << distanceTravelled;
	setDistance(_currentDistance + distanceTravelled);
}

void VideoController::setDistance(float distance)
{
	_currentDistance = distance;
	emit distanceChanged(distance);
}

void VideoController::loadVideo(const QString &filename)
{
	QMetaObject::invokeMethod(&_videoDecoder, "openFile",
							  Q_ARG(QString, filename));
}

void VideoController::setPosition(quint32 frameNr)
{
	QMetaObject::invokeMethod(&_videoDecoder, "seekFrame",
							  Q_ARG(quint32, frameNr));
}

void VideoController::reset()
{
	_updateTimer.stop();
	_playTimer.stop();
	_currentFrame = ImageFrame();
	_imageQueue.drain();
}
