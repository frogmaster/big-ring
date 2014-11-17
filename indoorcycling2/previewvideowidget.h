#ifndef PREVIEWVIDEOWIDGET_H
#define PREVIEWVIDEOWIDGET_H

#include <QtCore/QTimer>
#include <QtCore/QtGlobal>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsWidget>
#include <QGst/Message>
#include <QGst/Pipeline>

class PreviewVideoWidget: public QWidget
{
    Q_OBJECT
public:
    PreviewVideoWidget(QWidget* parent);
    virtual ~PreviewVideoWidget();
    void setUri(QString uri);
signals:
    void stateChanged();

public slots:
    void play();
    void step();
    void updateText();

protected:
    void resizeEvent(QResizeEvent *);
private:
    void onBusMessage(const QGst::MessagePtr & message);
    void handlePipelineStateChange(const QGst::StateChangedMessagePtr & scm);
    void stop();
    void seek();

    QGst::ElementPtr _videoSink;
    QGst::PipelinePtr _pipeline;
    QTimer* _stepTimer;
    QTimer* _textTimer;
    QGraphicsView* _graphicsView;
    QGraphicsWidget* _videoWidget;
    QGraphicsTextItem* _text;
    bool _seekDone;
};

#endif // PREVIEWVIDEOWIDGET_H
