#include "videoreader.h"

#include <cstring>

#include <QtCore/QCoreApplication>
#include <QtCore/QSize>
#include <QtCore/QtDebug>
#include <QtGui/QImage>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libswscale/swscale.h"
}

#include "reallifevideo.h"

namespace {
const int ERROR_STR_BUF_SIZE = 128;
QEvent::Type CreateImageForFrameEventType = static_cast<QEvent::Type>(QEvent::User + 102);

class CreateImageForFrameEvent: public QEvent
{
public:
    CreateImageForFrameEvent(const RealLifeVideo& rlv, qreal distance):
        QEvent(CreateImageForFrameEventType), _rlv(rlv), _distance(distance)
    {
        // empty
    }

    RealLifeVideo _rlv;
    qreal _distance;
};
}

VideoReader::VideoReader(QObject *parent) :
    GenericVideoReader(parent), _frameRgb(nullptr)
{
    // empty
}

VideoReader::~VideoReader()
{
    qDebug() << "closing Videoreader";
}

void VideoReader::createImageForFrame(const RealLifeVideo& rlv, const qreal distance)
{
    QCoreApplication::postEvent(this, new CreateImageForFrameEvent(rlv, distance));
}

void VideoReader::openVideoFileInternal(const QString &videoFilename)
{
    GenericVideoReader::openVideoFileInternal(videoFilename);

    _frameRgb.reset(new AVFrameWrapper);
    int numBytes= avpicture_get_size(PIX_FMT_RGB24,
          codecContext()->width, codecContext()->height);
    _imageBuffer.resize(numBytes);
    avpicture_fill(_frameRgb->asPicture(), reinterpret_cast<uint8_t*>(_imageBuffer.data()), PIX_FMT_RGB24,
                   codecContext()->width, codecContext()->height);

    _swsContext = sws_getContext(codecContext()->width, codecContext()->height, AV_PIX_FMT_YUV420P,
                                 codecContext()->width, codecContext()->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR,
                                 nullptr, nullptr, nullptr);

    emit videoOpened(videoFilename, QSize(codecContext()->width, codecContext()->height));
}

void VideoReader::createImageForFrameNumber(RealLifeVideo& rlv, const qreal distance)
{
    // the first few frames are sometimes black, so when requested to take a "screenshot" of the first frames, just
    // skip to a few frames after the start.
    qint64 frameNumber = qMax(20, static_cast<int>(rlv.frameForDistance(distance)));
    qDebug() << "creating image for" << rlv.name() << "distance" << distance << "frame nr" << frameNumber;
    performSeek(frameNumber);
    loadFramesUntilTargetFrame(frameNumber);
    emit newFrameReady(rlv, distance, createImage());
}

bool VideoReader::event(QEvent *event)
{
    if (event->type() == CreateImageForFrameEventType) {
        CreateImageForFrameEvent* createImageForFrameEvent = dynamic_cast<CreateImageForFrameEvent*>(event);

        RealLifeVideo& rlv = createImageForFrameEvent->_rlv;
        const qreal distance = createImageForFrameEvent->_distance;
        qDebug() << "creating thumbnail for rlv" << rlv.name();
        openVideoFileInternal(rlv.videoInformation().videoFilename());

        rlv.setNumberOfFrames(totalNumberOfFrames());
        createImageForFrameNumber(rlv, distance);
        return true;
    }
    return QObject::event(event);
}

QImage VideoReader::createImage()
{
    sws_scale(_swsContext, frameYuv().frame->data, frameYuv().frame->linesize, 0, codecContext()->height, _frameRgb->frame->data,
              _frameRgb->frame->linesize);
    QImage image(frameYuv().frame->width, frameYuv().frame->height, QImage::Format_RGB888);
    std::memcpy(image.bits(), _frameRgb->frame->data[0], _imageBuffer.size());
    return image;
}
