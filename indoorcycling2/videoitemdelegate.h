#ifndef VIDEOITEMDELEGATE_H
#define VIDEOITEMDELEGATE_H
#include <QtWidgets/QAbstractItemDelegate>
class RealLifeVideo;
class VideoItemDelegate: public QAbstractItemDelegate
{
    Q_OBJECT
public:
    VideoItemDelegate(QObject* parent);

    virtual void paint(QPainter* painter, const QStyleOptionViewItem&, const QModelIndex&) const;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:
    void paintProfile(QPainter* painter, QRect& rect, RealLifeVideo& rlv) const;
    QPixmap drawProfilePixmap(QRect &rect, RealLifeVideo& rlv) const;

    qreal distanceToX(const QRect& rect, const RealLifeVideo& rlv, float distance) const;
    float xToDistance(const QRect& rect, const RealLifeVideo& rlv, int x) const;
    int altitudeToHeight(const QRect& rect, float altitudeAboveMinimum, float altitudeDiff) const;
};

#endif // VIDEOITEMDELEGATE_H
