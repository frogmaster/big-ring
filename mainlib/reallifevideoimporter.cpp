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

#include "reallifevideoimporter.h"
#include "rlvfileparser.h"
#include "virtualcyclingfileparser.h"
#include "bigringsettings.h"

#include <functional>

#include <QtCore/QCoreApplication>
#include <QtCore/QDirIterator>
#include <QtCore/QEvent>
#include <QtCore/QSettings>

#include <QStringList>
#include <QtConcurrent/QtConcurrentMap>
#include <QtConcurrent/QtConcurrentRun>
#include <QtDebug>

namespace
{
RealLifeVideo parseRealLiveVideoFile(QFile &rlvFile, const QList<QString> &videoFilePaths, const QList<QString> &pgmfFilePaths);
QSet<QString> findFiles(const QString& root, const QStringList &patterns);
QSet<QString> findRlvFiles(const QString &root);

const QEvent::Type NR_OF_RLVS_FOUND_TYPE = static_cast<QEvent::Type>(QEvent::User + 100);
const QEvent::Type RLV_IMPORTED_TYPE = static_cast<QEvent::Type>(NR_OF_RLVS_FOUND_TYPE + 101);

class NrOfRlvsFoundEvent: public QEvent
{
public:
    NrOfRlvsFoundEvent(int nr): QEvent(NR_OF_RLVS_FOUND_TYPE), _nr(nr) {}

    int _nr;
};
class RlvImportedEvent: public QEvent
{
public:
    RlvImportedEvent(): QEvent(RLV_IMPORTED_TYPE) {}
};
}

RealLifeVideoImporter::RealLifeVideoImporter(QObject* parent): QObject(parent)
{
    // empty
}

RealLifeVideoImporter::~RealLifeVideoImporter()
{
    qDebug() << "deleting RealLifeVideoImporter";
}

void RealLifeVideoImporter::importRealLiveVideoFilesFromDir()
{
    QFutureWatcher<RealLifeVideoList> *futureWatcher = new QFutureWatcher<RealLifeVideoList>();
    connect(futureWatcher, &QFutureWatcher<RealLifeVideoList>::finished, futureWatcher, [=]() {
        importReady(futureWatcher->future().result());
        futureWatcher->deleteLater();
    });

    std::function<RealLifeVideoList(void)> importFunction([this]() {
       return this->importRlvFiles(BigRingSettings().videoFolder());
    });
    QFuture<QList<RealLifeVideo> > importRlvFuture = QtConcurrent::run(importFunction);
    futureWatcher->setFuture(importRlvFuture);
}

bool RealLifeVideoImporter::event(QEvent *event)
{
    if (event->type() == NR_OF_RLVS_FOUND_TYPE) {
        emit rlvFilesFound(dynamic_cast<NrOfRlvsFoundEvent*>(event)->_nr);
        return true;
    } else if (event->type() == RLV_IMPORTED_TYPE) {
        emit rlvImported();
        return true;
    } else {
        return QObject::event(event);
    }
}

RealLifeVideoList RealLifeVideoImporter::importRlvFiles(const QString& rootFolder)
{
    const QSet<QString> rlvFiles = findRlvFiles(rootFolder);
    qDebug() << "rlv files" << rlvFiles;
    const QSet<QString> pgmfFiles = findFiles(rootFolder, { "*.pgmf" });
    const QSet<QString> aviFiles = findFiles(rootFolder, { "*.avi" });

    QCoreApplication::postEvent(this, new NrOfRlvsFoundEvent(rlvFiles.size()));

    std::function<RealLifeVideo(const QFileInfo&)> importFunction([this, aviFiles, pgmfFiles](const QFileInfo& fileInfo) -> RealLifeVideo {
        QFile file(fileInfo.canonicalFilePath());
        RealLifeVideo rlv = parseRealLiveVideoFile(file, aviFiles.toList(), pgmfFiles.toList());
        QCoreApplication::postEvent(this, new RlvImportedEvent);
        return rlv;
    });

    return QtConcurrent::mapped(rlvFiles.begin(), rlvFiles.end(), importFunction).results();
}


void RealLifeVideoImporter::importReady(const RealLifeVideoList &rlvs)
{

    RealLifeVideoList validRlvs;

    for (RealLifeVideo rlv: rlvs) {
        if (rlv.isValid() && rlv.type() == ProfileType::SLOPE) {
            validRlvs.append(rlv);
        }
    }
    // sort rlv list by name
    qSort(validRlvs.begin(), validRlvs.end(), RealLifeVideo::compareByName);

    emit importFinished(validRlvs);
}

namespace
{

QList<QFileInfo> fromPaths(const QList<QString>& filePaths) {
    QList<QFileInfo> fileInfos;
    std::for_each(filePaths.constBegin(), filePaths.constEnd(), [&fileInfos](QString filePath) {
        fileInfos.append(QFileInfo(filePath));
    });
    return fileInfos;
}

RealLifeVideo parseRealLiveVideoFile(QFile &rlvFile, const QList<QString>& videoFilePaths,
                                     const QList<QString>& pgmfFilePaths)
{
    QList<QFileInfo> videoFiles = fromPaths(videoFilePaths);
    QList<QFileInfo> pgmfFiles = fromPaths(pgmfFilePaths);

    RealLifeVideo rlv;
    if (rlvFile.fileName().endsWith(".rlv")) {
        RlvFileParser parser(pgmfFiles, videoFiles);
        rlv = parser.parseRlvFile(rlvFile);
    } else if (rlvFile.fileName().endsWith(".xml")) {
        rlv = indoorcycling::VirtualCyclingFileParser(videoFiles).parseVirtualCyclingFile(rlvFile);
    }
    QSettings settings;
    settings.beginGroup(QString("%1.custom_courses").arg(rlv.name()));
    QStringList customCourseNames = settings.allKeys();
    for (QString customCourseName: customCourseNames) {
        int startDistance = settings.value(customCourseName).toInt();
        rlv.addStartPoint(startDistance, customCourseName);
    }
    settings.endGroup();
    settings.beginGroup("unfinished_runs");
    const QString key = rlv.name();
    if (settings.contains(key)) {
        float distance = settings.value(key).toFloat();
        rlv.setUnfinishedRun(distance);
    }
    settings.endGroup();

    return rlv;
}

QSet<QString> findFiles(const QString& root, const QStringList& patterns)
{
    QDirIterator it(root, patterns, QDir::NoFilter, QDirIterator::Subdirectories);

    QSet<QString> filePaths;

    while(it.hasNext()) {
        it.next();
        filePaths.insert(it.filePath());
    }
    return filePaths;
}

QSet<QString> findRlvFiles(const QString& root)
{
    return findFiles(root, { "*.rlv", "*.xml" } );
}

}
