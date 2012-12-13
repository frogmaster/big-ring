#include "reallivevideo.h"

#include <QtDebug>
#include <QMapIterator>
Course::Course():
	_start(0.0), _end(0.0)
{}

Course::Course(const QString &name, float start, float end):
	_name(name), _start(start), _end(end)
{}

RealLiveVideo::RealLiveVideo(const QString& name, const VideoInformation& videoInformation,
							 QList<Course>& courses, QList<DistanceMappingEntry> distanceMappings, QMap<float, ProfileEntry> profile):
	_name(name), _videoInformation(videoInformation), _courses(courses), _profile(profile)
{
	float currentDistance = 0.0f;

	quint32 lastFrameNumber = 0;
	QListIterator<DistanceMappingEntry> it(distanceMappings);
	while(it.hasNext()) {
		const DistanceMappingEntry& entry = it.next();
		quint32 nrFrames = entry.frameNumber() - lastFrameNumber;
		currentDistance += nrFrames * entry.metersPerFrame();

		_distanceMappings[currentDistance] = entry;

		lastFrameNumber = entry.frameNumber();
	}

}

RealLiveVideo::RealLiveVideo() {}

float RealLiveVideo::metersPerFrame(const float distance) const
{
	float keyDistance = findDistanceMappingEntryFor(distance);
	const DistanceMappingEntry entry = _distanceMappings[keyDistance];
	return entry.metersPerFrame();
}

quint32 RealLiveVideo::frameForDistance(const float distance) const
{
	float keyDistance = findDistanceMappingEntryFor(distance);
	const DistanceMappingEntry entry = _distanceMappings[keyDistance];

	return entry.frameNumber() + (distance - keyDistance) / entry.metersPerFrame();
}

float RealLiveVideo::slopeForDistance(const float distance) const
{
	float lastSlope = 0.0f;
	QMapIterator<float, ProfileEntry> it(_profile);
	while(it.hasNext()) {
		it.next();

		float currentDistance = it.key();
		ProfileEntry entry = it.value();
		if (currentDistance > distance)
			break;
		lastSlope = entry.slope();
	}
	return lastSlope;
}

VideoInformation::VideoInformation(const QString &videoFilename, float frameRate):
	_videoFilename(videoFilename), _frameRate(frameRate)
{
}

VideoInformation::VideoInformation():
	_frameRate(0.0) {}

bool RealLiveVideo::compareByName(const RealLiveVideo &rlv1, const RealLiveVideo &rlv2)
{
	return rlv1.name().toLower() < rlv2.name().toLower();
}

float RealLiveVideo::findDistanceMappingEntryFor(const float distance) const
{
	float lastDistance = 0.0f;
	QMapIterator<float, DistanceMappingEntry> it(_distanceMappings);
	while(it.hasNext()) {
		it.next();

		if (it.key() > distance)
			break;
		lastDistance = it.key();
	}
	return lastDistance;
}

DistanceMappingEntry::DistanceMappingEntry(quint32 frameNumber, float metersPerFrame):
	_frameNumber(frameNumber), _metersPerFrame(metersPerFrame)
{
}

DistanceMappingEntry::DistanceMappingEntry():
	_frameNumber(0), _metersPerFrame(0.0f)
{
}


ProfileEntry::ProfileEntry(float distance, float slope):
	_distance(distance), _slope(slope)
{
}

ProfileEntry::ProfileEntry():
	_distance(.0f), _slope(.0f)
{
}


