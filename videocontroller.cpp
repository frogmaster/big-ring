#include "videocontroller.h"

#include <QtDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>

#include "videowidget.h"

namespace {
const quint32 NR_FRAMES_PER_REQUEST = 11;
const int NR_FRAMES_BUFFER_LOW = 10;
const int FRAME_INTERVAL = 1000/30;
}

VideoController::VideoController(Cyclist &cyclist, VideoWidget* videoWidget, QObject *parent) :
	QObject(parent),
	_cyclist(cyclist),
	_videoDecoder(new VideoDecoder(this)),
	_videoWidget(videoWidget),
	_lastFrameRateSample(QDateTime::currentDateTime()),
	_currentFrameRate(0u)
{
	// set up timers
	_playTimer.setInterval(FRAME_INTERVAL);
	connect(&_playTimer, SIGNAL(timeout()), SLOT(playNextFrame()));

	// set up video decoder
	connect(_videoDecoder, SIGNAL(videoLoaded()), SLOT(videoLoaded()));
	connect(_videoDecoder, SIGNAL(framesReady(FrameList)), SLOT(framesReady(FrameList)));
	connect(_videoDecoder, SIGNAL(seekFinished(Frame)), SLOT(seekFinished(Frame)));
}

VideoController::~VideoController()
{
	delete _videoDecoder;
}

bool VideoController::isBufferFull()
{
	return (_loadedFrameNumber > _currentFrameNumber) || (_loadedFrameNumber > 0 && _currentFrameNumber == UNKNOWN_FRAME_NR);
}

void VideoController::offerFrame(Frame& frame)
{
	loadFrame(frame);
	emit bufferFull(true);
}

void VideoController::realLiveVideoSelected(RealLiveVideo rlv)
{
	reset();
	_currentRlv = rlv;
	if (!_currentRlv.videoInformation().videoFilename().isEmpty())
		loadVideo(_currentRlv.videoInformation().videoFilename());
}

void VideoController::courseSelected(int courseNr)
{
	reset();

	if (courseNr == -1) {
		return;
	}

	if (!_currentRlv.isValid())
		return;

	quint32 frame = _currentRlv.frameForDistance(_cyclist.distance());

	setPosition(frame);
}

void VideoController::play(bool doPlay)
{
	if (doPlay) {
		_playTimer.start();
	} else {
		_playTimer.stop();
	}
	emit playing(_playTimer.isActive());
}

void VideoController::videoLoaded()
{
	qDebug() << "video loaded";
}

void VideoController::playNextFrame()
{
	// keep track of frame rate
	QDateTime now = QDateTime::currentDateTime();
	if (_lastFrameRateSample.addSecs(1) < now) {
		_currentFrameRate = _framesThisSecond;
		_framesThisSecond = 0;
		_lastFrameRateSample = now;
		qDebug() << "framerate = " << _currentFrameRate;
		emit(currentFrameRate(_currentFrameRate));
	}

	quint32 frameToShow = _currentRlv.frameForDistance(_cyclist.distance());
	displayFrame(frameToShow);
}

void VideoController::displayFrame(quint32 frameToShow)
{
	_videoWidget->displayNextFrame(frameToShow);
	_framesThisSecond += frameToShow - _currentFrameNumber;
	_currentFrameNumber = frameToShow;

//	if (frameToShow == _currentFrameNumber)
//		return; // no need to display again.
//	if (frameToShow < _currentFrameNumber && _currentFrameNumber != UNKNOWN_FRAME_NR) {
//		qDebug() << "frame to show" << frameToShow << "current" << _currentFrameNumber;
//		return; // wait until playing catches up.
//	}

//	// if we've loaded a frame that's higher than the currently shown frame, let the widget display it.
//	if (_loadedFrameNumber > _currentFrameNumber || _currentFrameNumber == UNKNOWN_FRAME_NR) {
//		if (_currentFrameNumber == UNKNOWN_FRAME_NR) {
//			_framesThisSecond += 1;
//		} else {
//			qDebug() << frameToShow << _currentFrameNumber ;
//			_framesThisSecond += (frameToShow -_currentFrameNumber);
//		}
//		_videoWidget->displayNextFrame(frameToShow);
//		_currentFrameNumber = _loadedFrameNumber;

//	}
	fillFrameBuffers();
}

void VideoController::seekFinished(Frame frame)
{
	loadFrame(frame);
	displayFrame(frame.frameNr);
	fillFrameBuffers();
}

void VideoController::loadFrame(Frame &frame)
{
	_loadedFrameNumber = frame.frameNr;
	_videoWidget->loadFrame(frame);
}

void VideoController::loadVideo(const QString &filename)
{
	QMetaObject::invokeMethod(_videoDecoder, "openFile",
							  Q_ARG(QString, filename));
}

void VideoController::setPosition(quint32 frameNr)
{
	QMetaObject::invokeMethod(_videoDecoder, "seekFrame",
							  Q_ARG(quint32, frameNr));
}

void VideoController::reset()
{
	qDebug() << "reset";
	_videoWidget->clearOpenGLBuffers();
	play(false);
	emit bufferFull(false);
	_playTimer.stop();
	_currentFrameNumber = UNKNOWN_FRAME_NR;
	_loadedFrameNumber = 0;
}

int VideoController::determineFramesToSkip()
{
	if (_currentFrameRate < 30)
		return 0; // skip no frames when (virtual) frame rate below 30 frames/s.
	else if (_currentFrameRate < 40)
		return 1; // skip 1 frame for every decoded frame.
	else
		return 2; // skip 2 frames for every decoded frame
}

void VideoController::fillFrameBuffers()
{
	int skip = determineFramesToSkip();
	while (!_videoWidget->buffersFull()) {
		_videoDecoder->loadFrames(skip);
	}
}
