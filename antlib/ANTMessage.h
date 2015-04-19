/*
 * Copyright (c) 2009 Mark Rages
 * Copyright (c) 2011 Mark Liversedge (liversedge@gmail.com)
 * Copyright (c) 2012-2015 Ilja Booij (ibooij@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "ANT.h"
#include "ANTChannel.h"

#ifndef gc_ANTMessage_h
#define gc_ANTMessage_h

extern "C" {
#include <stdint.h>
}

enum AntDataType : quint8 {
    AntSystemReset = ANT_SYSTEM_RESET,
    AntSyncByte = ANT_SYNC_BYTE,
    AntSetNetworkKey = ANT_SET_NETWORK
};

class ANTMessage {

    public:

        ANTMessage(); // null message
        ANTMessage(const QByteArray& messageBytes);
        ANTMessage(unsigned char b1,
                   unsigned char b2 = '\0',
                   unsigned char b3 = '\0',
                   unsigned char b4 = '\0',
                   unsigned char b5 = '\0',
                   unsigned char b6 = '\0',
                   unsigned char b7 = '\0',
                   unsigned char b8 = '\0',
                   unsigned char b9 = '\0',
                   unsigned char b10 = '\0',
                   unsigned char b11 = '\0'); // encode with values (at least one value must be passed though)

        // convenience functions for encoding messages
        static ANTMessage requestCalibrate(const unsigned char channel);

        // convert a channel event message id to human readable string
        static const char * channelEventMessage(unsigned char c);

        // to avoid a myriad of tedious set/getters the data fields
        // are plain public members. This is unlikely to change in the
        // future since the whole purpose of this class is to encode
        // and decode ANT messages into the member variables below.

        // BEAR IN MIND THAT ONLY A HANDFUL OF THE VARS WILL BE SET
        // DURING DECODING, SO YOU *MUST* EXAMINE THE MESSAGE TYPE
        // TO DECIDE WHICH FIELDS TO REFERENCE

        // Standard ANT values
        int length;
        unsigned char data[ANT_MAX_MESSAGE_SIZE+1]; // include sync byte at front
        qint64 timestamp;
        uint8_t sync, type;
        uint8_t transmitPower, searchTimeout, transmissionType, networkNumber,
                channelType, channel, deviceType, frequency;
        uint16_t channelPeriod, deviceNumber;
        uint8_t key[8];

        // ANT Sport values
        uint8_t data_page, calibrationID, ctfID;
        uint16_t srmOffset, srmSlope, srmSerial;
        uint8_t eventCount;
        uint8_t pedalPower;
        uint16_t measurementTime, wheelMeasurementTime, crankMeasurementTime;
        uint8_t heartrateBeats, instantHeartrate; // heartrate
        uint16_t slope, period, torque; // power
        uint16_t sumPower, instantPower; // power
        uint16_t wheelRevolutions, crankRevolutions; // speed and power and cadence
        uint8_t instantCadence; // cadence
        uint8_t autoZeroStatus, autoZeroEnable;

        QString toString() const;
        int messageType() const;
        /** all bytes of the message */
        QByteArray bytes() const;
        /** payload length of the message */
        int payloadLength() const;
        /** the payload bytes of the message */
        QByteArray payLoad() const;
        ANTMessage(int channelType, const unsigned char *data); // decode from parameters
private:
        QString channelEventToString() const;
        uint8_t payLoadByte(int byteNr) const;
        uint16_t payLoadShort(int index) const;

        static float timeOutToSeconds(const uint8_t timeoutValue);
        void init();
};
#endif
