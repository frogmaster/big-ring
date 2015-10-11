/*
 * Copyright (c) 2015 Ilja Booij (ibooij@gmail.com)
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
#include "profilepainter.h"

#include <QtCore/QtDebug>
#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>

#include <array>
namespace
{
const int MAXIMUM_HUE = 240; // dark blue;
const float MINIMUM_SLOPE = -12.0;
const float MAXIMUM_SLOPE = 12.0;
const float INVERSE_SLOPE_RANGE = 1 / (MAXIMUM_SLOPE - MINIMUM_SLOPE);
const float METERS_PER_MILE = 1609.344;

const std::array<float,6> MARKER_DISTANCES = {1, 2, 5, 10, 20, 50};
}

ProfilePainter::ProfilePainter(QObject *parent) :
    QObject(parent), _quantityPrinter(new QuantityPrinter(this))
{
    if (_quantityPrinter == nullptr) {
        qDebug() << "NULL?";
    } else {
        qDebug() << "Quantity printer is not null";
    }
}

QPixmap ProfilePainter::paintProfile(const RealLifeVideo &rlv, const QRect &rect, bool withMarkers) const
{
    QPixmap profilePixmap;
    if (rlv.isValid()) {
        QRect profileRect = QRect(QPoint(0,0),rect.size());

        const QString pixmapName = QString("%1_%2x%3").arg(rlv.name()).arg(rect.size().width()).arg(rect.size().height());
        if (!QPixmapCache::find(pixmapName, &profilePixmap)) {
            qDebug() << "creating new profile pixmap for" << pixmapName;
            profilePixmap = drawProfilePixmap(profileRect, rlv, withMarkers);
            QPixmapCache::insert(pixmapName, profilePixmap);
        }
    }
    return profilePixmap;
}

QPixmap ProfilePainter::paintProfileWithHighLight(const RealLifeVideo &rlv, qreal startDistance, qreal endDistance,
                                                  const QRect &rect, const QBrush highlightColor) const
{
    QPixmap profilePixmap = paintProfile(rlv, rect, true);
    if (profilePixmap.isNull()) {
        return profilePixmap;
    }

    int startX = distanceToX(rect, rlv, startDistance);
    int endX = distanceToX(rect, rlv, endDistance);

    QPixmap copy(profilePixmap);
    QPainter painter(&copy);

    QColor color(highlightColor.color());
    color.setAlphaF(0.5);
    painter.setBrush(color);
    painter.drawRect(startX, 0, endX - startX, rect.height());
    painter.end();

    return copy;
}

QPixmap ProfilePainter::drawProfilePixmap(QRect& rect, const RealLifeVideo& rlv, bool withMarkers ) const
{
    if (rect.isEmpty()) {
        return QPixmap();
    }

    QPixmap pixmap(rect.size());
    QPainter painter(&pixmap);

    float minimumAltitude = rlv.profile().minimumAltitude();
    float maximumAltitude = rlv.profile().maximumAltitude();

    float altitudeDiff = maximumAltitude - minimumAltitude;
    painter.setBrush(Qt::gray);
    painter.drawRect(rect);
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);

    for(int x = 0; x < rect.width(); x += 1) {
        float distance = xToDistance(rect, rlv, x);
        float altitude = rlv.profile().altitudeForDistance(distance);
        painter.setBrush(colorForSlope(rlv.profile().slopeForDistance(distance)));

        int y = altitudeToHeight(rect, altitude - minimumAltitude, altitudeDiff);
        QRect  box(x, rect.bottom() - y, 1, rect.bottom());
        painter.drawRect(box);
    }
    if (withMarkers) {
        drawDistanceMarkers(painter, rect, rlv);
    }
    painter.end();
    return pixmap;
}

void ProfilePainter::drawDistanceMarkers(QPainter &painter, const QRect &rect, const RealLifeVideo &rlv) const
{
    QPen pen(Qt::black, 1);
    painter.setPen(pen);
    qDebug() << static_cast<int>(_quantityPrinter->system());
    const float distanceBetweenMarkers = determineDistanceMarkers(rlv);

    for (float distance = distanceBetweenMarkers; distance < rlv.totalDistance(); distance += distanceBetweenMarkers) {
        int x = distanceToX(rect, rlv, distance);
        painter.drawLine(x, rect.height () - 20, x, rect.height());
        QString distanceMarker = QString("%1 %2").arg(_quantityPrinter->printDistance(distance))
                .arg(_quantityPrinter->unitForDistance(QuantityPrinter::Precision::Precise,
                                                       QVariant::fromValue(distance)));
        painter.drawText(x, rect.height(), distanceMarker);
    }
}

float ProfilePainter::determineDistanceMarkers(const RealLifeVideo &rlv) const
{
    std::array<float, 6> distances;
    std::transform(MARKER_DISTANCES.begin(), MARKER_DISTANCES.end(), distances.begin(), [this](float distance) {
        if (_quantityPrinter->system() == QuantityPrinter::System::Metric) {
            return distance * 1000;
        } else {
            return distance * METERS_PER_MILE;
        }
    });
    const float optimalDistanceBetweenMarkers = rlv.totalDistance() / 10;
    float distanceBetweenMarkers;
    auto distanceBetweenMarkersIt = std::lower_bound(distances.begin(), distances.end(), optimalDistanceBetweenMarkers);
    if (distanceBetweenMarkersIt == MARKER_DISTANCES.end()) {
        distanceBetweenMarkers = 50;
    } else {
        distanceBetweenMarkers = *distanceBetweenMarkersIt;
    }
    return distanceBetweenMarkers;

}



qreal ProfilePainter::distanceToX(const QRect& rect, const RealLifeVideo& rlv, float distance) const
{
    return (distance / rlv.totalDistance()) * rect.width() + rect.left();
}

float ProfilePainter::xToDistance(const QRect& rect, const RealLifeVideo& rlv, int x) const
{
    float relative = x * 1.0 / rect.width();
    return rlv.totalDistance() * relative;
}

int ProfilePainter::altitudeToHeight(const QRect& rect, float altitudeAboveMinimum, float altitudeDiff) const
{
    return static_cast<int>(((altitudeAboveMinimum) / altitudeDiff) * rect.height() * .9);
}

QColor ProfilePainter::colorForSlope(const float slope) const {
    const float boundedSlope = qBound(MINIMUM_SLOPE, slope, MAXIMUM_SLOPE);
    /* 0 is MINIMUM_SLOPE or lower, 1 = MAXIMUM_SLOPE or higher*/
    const float normalizedSlope = (boundedSlope - MINIMUM_SLOPE) * INVERSE_SLOPE_RANGE;

    return QColor::fromHsv(MAXIMUM_HUE - (normalizedSlope * MAXIMUM_HUE), 255, 255);
}

