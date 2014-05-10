/*
Copyright 2012  Peter Simonsson <peter.simonsson@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "qatemconnection.h"

#include <QDebug>
#include <QTimer>
#include <QHostAddress>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <QCryptographicHash>

#include <math.h>

#define SIZE_OF_HEADER 0x0c

QAtemConnection::QAtemConnection(QObject* parent)
    : QObject(parent)
{
    m_socket = new QUdpSocket(this);
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(m_socket, SIGNAL(readyRead()),
            this, SLOT(handleSocketData()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handleError(QAbstractSocket::SocketError)));

    m_connectionTimer = new QTimer(this);
    m_connectionTimer->setInterval(1000);
    connect(m_connectionTimer, SIGNAL(timeout()),
            this, SLOT(handleConnectionTimeout()));

    m_port = 9910;
    m_packetCounter = 0;
    m_isInitialized = false;
    m_currentUid = 0;

    m_debugEnabled = false;

    m_programInput = 0;
    m_previewInput = 0;
    m_tallyStateCount = 0;

    m_transitionPreviewEnabled = false;
    m_transitionFrameCount = 0;
    m_transitionPosition = 0;
    m_keyersOnCurrentTransition = 0;
    m_currentTransitionStyle = 0;
    m_keyersOnNextTransition = 0;
    m_nextTransitionStyle = 0;

    m_fadeToBlackEnabled = false;
    m_fadeToBlackFading = false;
    m_fadeToBlackFrameCount = 0;

    m_mixFrames = 0;

    m_dipFrames = 0;
    m_dipSource = 0;

    m_wipeFrames = 0;
    m_wipeBorderSource = 0;
    m_wipeBorderWidth = 0;
    m_wipeBorderSoftness = 0;
    m_wipeType = 0;
    m_wipeSymmetry = 0;
    m_wipeXPosition = 0;
    m_wipeYPosition = 0;
    m_wipeReverseDirection = false;
    m_wipeFlipFlop = false;

    m_dveRate = 0;
    m_dveEffect = 0;
    m_dveFillSource = 0;
    m_dveKeySource = 0;
    m_dveEnableKey = false;
    m_dveEnablePreMultipliedKey = false;
    m_dveKeyClip = 0;
    m_dveKeyGain = 0;
    m_dveEnableInvertKey = false;
    m_dveReverseDirection = false;
    m_dveFlipFlopDirection = false;

    m_stingerSource = 0;
    m_stingerEnablePreMultipliedKey = false;
    m_stingerClip = 0;
    m_stingerGain = 0;
    m_stingerEnableInvertKey = false;
    m_stingerPreRoll = 0;
    m_stingerClipDuration = 0;
    m_stingerTriggerPoint = 0;
    m_stingerMixRate = 0;

    m_multiViewLayout = 0;

    m_videoFormat = 0;
    m_videoDownConvertType = 0;

    m_mediaPoolClip1Size = 0;
    m_mediaPoolClip2Size = 0;

    m_majorversion = 0;
    m_minorversion = 0;

    m_audioMonitorEnabled = false;
    m_audioMonitorGain = 0;
    m_audioMonitorDimmed = false;
    m_audioMonitorMuted = false;
    m_audioMonitorSolo = -1;

    m_audioMasterOutputLevelLeft = 0;
    m_audioMasterOutputLevelRight = 0;
    m_audioMasterOutputGain = 0;

    m_transferActive = false;
    m_transferStoreId = 0;
    m_transferIndex = 0;
    m_transferId = 0;
    m_lastTransferId = 0;

    initCommandSlotHash();
}

void QAtemConnection::connectToSwitcher(const QHostAddress &address)
{
    m_address = address;

    if(m_address.isNull())
    {
        return;
    }

    m_socket->bind();
    m_packetCounter = 0;
    m_isInitialized = false;
    m_currentUid = 0x1337; // Just a random UID, we'll get a new one from the server eventually

    //Hello
    QByteArray datagram = createCommandHeader(Cmd_HelloPacket, 8, m_currentUid, 0x0);
    datagram.append(QByteArray::fromHex("0100000000000000")); // The Hello package needs this... no idea what it means

    sendDatagram(datagram);
    m_connectionTimer->start();
}

void QAtemConnection::disconnectFromSwitcher()
{
    m_socket->close();
    m_connectionTimer->stop();
}

void QAtemConnection::handleSocketData()
{
    while (m_socket->hasPendingDatagrams())
    {
        m_connectionTimer->start();
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());

        m_socket->readDatagram(datagram.data(), datagram.size());

//        qDebug() << datagram.toHex();

        QAtemConnection::CommandHeader header = parseCommandHeader(datagram);
        m_currentUid = header.uid;

        if(header.bitmask & Cmd_HelloPacket)
        {
            m_isInitialized = false;
            QByteArray ackDatagram = createCommandHeader(Cmd_Ack, 0, header.uid, 0x0);
            sendDatagram(ackDatagram);
        }
        else if(m_isInitialized && (header.bitmask & Cmd_AckRequest))
        {
            QByteArray ackDatagram = createCommandHeader(Cmd_Ack, 0, header.uid, header.packageId);
            sendDatagram(ackDatagram);
            m_socket->flush();
        }

        if(datagram.size() > (SIZE_OF_HEADER + 2) && !(header.bitmask & (Cmd_HelloPacket | Cmd_Resend)))
        {
            parsePayLoad(datagram);
        }

        m_socket->flush();
    }
}

QByteArray QAtemConnection::createCommandHeader(Commands bitmask, quint16 payloadSize, quint16 uid, quint16 ackId)
{
    QByteArray buffer(12, (char)0x0);
    quint16 packageId = 0;

    if(!(bitmask & (Cmd_HelloPacket | Cmd_Ack)))
    {
        m_packetCounter++;
        packageId = m_packetCounter;
    }

    U16_U8 val;

    val.u16 = bitmask;
    val.u16 = val.u16 << 11;
    val.u16 |= (payloadSize + SIZE_OF_HEADER);
    buffer[0] = (char)val.u8[1];
    buffer[1] = (char)val.u8[0];

    val.u16 = uid;
    buffer[2] = (char)val.u8[1];
    buffer[3] = (char)val.u8[0];

    val.u16 = ackId;
    buffer[4] = (char)val.u8[1];
    buffer[5] = (char)val.u8[0];

    val.u16 = packageId;
    buffer[10] = (char)val.u8[1];
    buffer[11] = (char)val.u8[0];

    return buffer;
}

QAtemConnection::CommandHeader QAtemConnection::parseCommandHeader(const QByteArray& datagram) const
{
    QAtemConnection::CommandHeader header;

    if(datagram.size() >= SIZE_OF_HEADER)
    {
        header.bitmask = (quint8)datagram[0] >> 3;
        header.size = (quint8)datagram[1] | ((quint8)(datagram[0] & 0x7) << 8);
        header.uid = (quint8)datagram[3] + ((quint8)datagram[2] << 8);
        header.ackId = (quint8)datagram[5] | ((quint8)datagram[4] << 8);
        // We don't try to parse 6-9 as we have no idea what it means
        header.packageId = (quint8)datagram[11] | ((quint8)datagram[10] << 8);
    }

    return header;
}

void QAtemConnection::parsePayLoad(const QByteArray& datagram)
{
    quint16 offset = SIZE_OF_HEADER;
    quint16 size = (quint8)datagram[offset + 1] | ((quint8)datagram[offset] << 8);

    while((offset + size) <= datagram.size())
    {
        QByteArray payload = datagram.mid(offset + 2, size - 2);
//        qDebug() << payload.toHex();

        QByteArray cmd = payload.mid(2, 4); // Skip first two bytes, not sure what they do

        if(cmd == "InCm")
        {
            m_isInitialized = true;
            QMetaObject::invokeMethod(this, "emitConnectedSignal", Qt::QueuedConnection);
        }
        else if(m_commandSlotHash.contains(cmd))
        {
            QMetaObject::invokeMethod(this, m_commandSlotHash.value(cmd), Qt::QueuedConnection, Q_ARG(QByteArray, payload));
        }
        else if(m_debugEnabled)
        {
            QString dbg;

            for(int i = 0; i < payload.size(); ++i)
            {
                uchar ch = payload[i];
                if(ch > 31)
                {
                    dbg.append(QChar(ch));
                }
                else
                {
                    dbg.append(QString::number(ch, 16));
                }

                dbg.append(" ");
            }

            qDebug() << dbg;
        }

        offset += size;

        if((offset + 2) < datagram.size())
        {
            size = (quint8)datagram[offset + 1] | ((quint8)datagram[offset] << 8);
        }
    }
}

void QAtemConnection::emitConnectedSignal()
{
    emit connected();
}

bool QAtemConnection::sendDatagram(const QByteArray& datagram)
{
    qint64 sent = m_socket->writeDatagram(datagram, m_address, m_port);

    return sent != -1;
}

bool QAtemConnection::sendCommand(const QByteArray& cmd, const QByteArray& payload)
{
    U16_U8 size;

    size.u16 = payload.size() + cmd.size() + 4;

    QByteArray datagram = createCommandHeader(Cmd_AckRequest, size.u16, m_currentUid, 0x0);

    datagram.append(size.u8[1]);
    datagram.append(size.u8[0]);

    datagram.append((char)0);
    datagram.append((char)0);

    datagram.append(cmd);
    datagram.append(payload);

    return sendDatagram(datagram);
}

void QAtemConnection::handleError(QAbstractSocket::SocketError)
{
    m_socket->close();
    m_connectionTimer->stop();
    emit socketError(m_socket->errorString());
    emit disconnected();
}

void QAtemConnection::handleConnectionTimeout()
{
    m_socket->close();
    m_connectionTimer->stop();
    emit socketError(tr("The switcher connection timed out"));
    emit disconnected();
}

void QAtemConnection::changeProgramInput(quint16 index)
{
    if(index == m_programInput)
    {
        return;
    }

    QByteArray cmd("CPgI");
    QByteArray payload(4, (char)0x0);
    U16_U8 val;

    val.u16 = index;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::changePreviewInput(quint16 index)
{
    if(index == m_previewInput)
    {
        return;
    }

    QByteArray cmd("CPvI");
    QByteArray payload(4, (char)0x0);
    U16_U8 val;

    val.u16 = index;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::doCut()
{
    QByteArray cmd("DCut");
    QByteArray payload(4, (char)0x0);

    sendCommand(cmd, payload);
}

void QAtemConnection::doAuto()
{
    QByteArray cmd("DAut");
    QByteArray payload(4, (char)0x0);

    sendCommand(cmd, payload);
}

void QAtemConnection::toggleFadeToBlack()
{
    QByteArray cmd("FtbA");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)0x02; // Does not toggle without this set

    sendCommand(cmd, payload);
}

void QAtemConnection::setFadeToBlackFrameRate(quint8 frames)
{
    if(frames == m_fadeToBlackFrameCount)
    {
        return;
    }

    QByteArray cmd("FtbC");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)frames;

    sendCommand(cmd, payload);
}

void QAtemConnection::setTransitionPosition(quint16 position)
{
    if(position == m_transitionPosition)
    {
        return;
    }

    QByteArray cmd("CTPs");
    QByteArray payload(4, (char)0x0);
    U16_U8 val;

    val.u16 = position;

    payload[1] = (char)0xe4;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::signalTransitionPositionChangeDone()
{
    QByteArray cmd("CTPs");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)0xf6;

    sendCommand(cmd, payload);
}

void QAtemConnection::setTransitionPreview(bool state)
{
    if(state == m_transitionPreviewEnabled)
    {
        return;
    }

    QByteArray cmd("CTPr");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)state;

    sendCommand(cmd, payload);
}

void QAtemConnection::setTransitionType(quint8 type)
{
    QByteArray cmd("CTTp");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)type;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyOn(quint8 keyer, bool state)
{
    if(state == m_upstreamKeys.value(keyer).m_onAir)
    {
        return;
    }

    QByteArray cmd("CKOn");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)keyer;
    payload[2] = (char)state;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyOnNextTransition(quint8 keyer, bool state)
{
    setKeyOnNextTransition(keyer + 1, state);
}

void QAtemConnection::setBackgroundOnNextTransition(bool state)
{
    setKeyOnNextTransition(0, state);
}

void QAtemConnection::setKeyOnNextTransition (int index, bool state)
{
    QByteArray cmd("CTTp");
    QByteArray payload(4, (char)0x0);

    quint8 stateValue = keyersOnNextTransition();

    if(state)
    {
        stateValue |= (0x1 << index);
    }
    else
    {
        stateValue &= (~(0x1 << index));
    }

    if(stateValue == keyersOnNextTransition())
    {
        return;
    }
    else if(stateValue == 0)
    {
        emit keyersOnNextTransitionChanged(keyersOnNextTransition());
        return;
    }

    payload[0] = (char)0x02;
    payload[3] = (char)stateValue & 0x1f;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyOn(quint8 keyer, bool state)
{
    if(state == m_downstreamKeys.value(keyer).m_onAir)
    {
        return;
    }

    QByteArray cmd("CDsL");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)keyer;
    payload[1] = (char)state;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyTie(quint8 keyer, bool state)
{
    if(state == m_downstreamKeys.value(keyer).m_tie)
    {
        return;
    }

    QByteArray cmd("CDsT");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)keyer;
    payload[1] = (char)state;

    sendCommand(cmd, payload);
}

void QAtemConnection::doDownstreamKeyAuto(quint8 keyer)
{
    QByteArray cmd("DDsA");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)keyer;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyFillSource(quint8 keyer, quint16 source)
{
    if(source == m_downstreamKeys.value(keyer).m_fillSource)
    {
        return;
    }

    QByteArray cmd("CDsF");
    QByteArray payload(4, (char)0x0);
    U16_U8 val;

    payload[0] = (char)keyer;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyKeySource(quint8 keyer, quint16 source)
{
    if(source == m_downstreamKeys.value(keyer).m_keySource)
    {
        return;
    }

    QByteArray cmd("CDsC");
    QByteArray payload(4, (char)0x0);
    U16_U8 val;

    payload[0] = (char)keyer;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyFrameRate(quint8 keyer, quint8 frames)
{
    if(frames == m_downstreamKeys.value(keyer).m_frames)
    {
        return;
    }

    QByteArray cmd("CDsR");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)keyer;
    payload[1] = (char)frames;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyInvertKey(quint8 keyer, bool invert)
{
    if(invert == m_downstreamKeys.value(keyer).m_invertKey)
    {
        return;
    }

    QByteArray cmd("CDsG");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x08;
    payload[1] = (char)keyer;
    payload[8] = (char)invert;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyPreMultiplied(quint8 keyer, bool preMultiplied)
{
    if(preMultiplied == m_downstreamKeys.value(keyer).m_preMultiplied)
    {
        return;
    }

    QByteArray cmd("CDsG");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)keyer;
    payload[2] = (char)preMultiplied;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyClip(quint8 keyer, float clip)
{
    if(clip == m_downstreamKeys.value(keyer).m_clip)
    {
        return;
    }

    QByteArray cmd("CDsG");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;
    val.u16 = clip * 10;

    payload[0] = (char)0x02;
    payload[1] = (char)keyer;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyGain(quint8 keyer, float gain)
{
    if(gain == m_downstreamKeys.value(keyer).m_gain)
    {
        return;
    }

    QByteArray cmd("CDsG");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;
    val.u16 = gain * 10;

    payload[0] = (char)0x04;
    payload[1] = (char)keyer;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyEnableMask(quint8 keyer, bool enable)
{
    if(enable == m_downstreamKeys.value(keyer).m_enableMask)
    {
        return;
    }

    QByteArray cmd("CDsM");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)keyer;
    payload[2] = (char)enable;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyMask(quint8 keyer, float top, float bottom, float left, float right)
{
    QByteArray cmd("CDsM");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x1e;
    payload[1] = (char)keyer;
    U16_U8 val;
    val.u16 = top * 1000;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];
    val.u16 = bottom * 1000;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];
    val.u16 = left * 1000;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];
    val.u16 = right * 1000;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::saveSettings()
{
    QByteArray cmd("SRsv");
    QByteArray payload(4, (char)0x0);

    sendCommand(cmd, payload);
}

void QAtemConnection::clearSettings()
{
    QByteArray cmd("SRcl");
    QByteArray payload(4, (char)0x0);

    sendCommand(cmd, payload);
}

void QAtemConnection::setColorGeneratorColor(quint8 generator, const QColor& color)
{
    if(color == m_colorGeneratorColors.value(generator))
    {
        return;
    }

    QByteArray cmd("CClV");
    QByteArray payload(8, (char)0x0);

    U16_U8 h, s, l;
    h.u16 = (qMax((qreal)0.0, color.hslHueF()) * 360.0) * 10;
    s.u16 = color.hslSaturationF() * 1000;
    l.u16 = color.lightnessF() * 1000;

    payload[0] = (char)0x07;
    payload[1] = (char)generator;
    payload[2] = (char)h.u8[1];
    payload[3] = (char)h.u8[0];
    payload[4] = (char)s.u8[1];
    payload[5] = (char)s.u8[0];
    payload[6] = (char)l.u8[1];
    payload[7] = (char)l.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setMediaPlayerSource(quint8 player, bool clip, quint8 source)
{
    QByteArray cmd("MPSS");
    QByteArray payload(8, 0);

    payload[1] = (char)player;

    if(clip) // Not available on TVS
    {
        payload[0] = (char)0x04;

        if(source <= 1)
        {
            payload[4] = (char)source;
        }
    }
    else
    {
        payload[0] = (char)0x02;

        if(source <= 31) // Only 20 sources on TVS
        {
            payload[3] = (char)source;
        }
    }

    sendCommand(cmd, payload);

    payload[0] = (char)0x01;
    payload[2] = (char)clip ? 2 : 1;
    payload[3] = (char)0xbf;
    payload[4] = (char)0x00;

    sendCommand(cmd, payload);
}

void QAtemConnection::setMediaPlayerLoop(quint8 player, bool loop)
{
    QByteArray cmd("SCPS");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x02;
    payload[1] = (char)player;
    payload[3] = (char)loop;

    sendCommand(cmd, payload);
}

void QAtemConnection::setMediaPlayerPlay(quint8 player, bool play)
{
    QByteArray cmd("SCPS");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)player;
    payload[2] = (char)play;

    sendCommand(cmd, payload);
}

void QAtemConnection::mediaPlayerGoToBeginning(quint8 player)
{
    QByteArray cmd("SCPS");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x04;
    payload[1] = (char)player;
    payload[4] = (char)0x01;

    sendCommand(cmd, payload);
}

void QAtemConnection::mediaPlayerGoFrameBackward(quint8 player)
{
    QByteArray cmd("SCPS");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x08;
    payload[1] = (char)player;

    sendCommand(cmd, payload);
}

void QAtemConnection::mediaPlayerGoFrameForward(quint8 player)
{
    QByteArray cmd("SCPS");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x08;
    payload[1] = (char)player;
    payload[7] = (char)0x01;

    sendCommand(cmd, payload);
}

quint8 QAtemConnection::tallyState(quint8 id) const
{
    if(id < m_tallyStateCount)
    {
        return m_tallyStates.value(id);
    }

    return 0;
}

bool QAtemConnection::downstreamKeyOn(quint8 keyer) const
{
    return m_downstreamKeys.value(keyer).m_onAir;
}

bool QAtemConnection::downstreamKeyTie(quint8 keyer) const
{
    return m_downstreamKeys.value(keyer).m_tie;
}

bool QAtemConnection::upstreamKeyOn(quint8 keyer) const
{
    return m_upstreamKeys.value(keyer).m_onAir;
}

QColor QAtemConnection::colorGeneratorColor(quint8 generator) const
{
    return m_colorGeneratorColors.value(generator);
}

quint8 QAtemConnection::mediaPlayerType(quint8 player) const
{
    return m_mediaPlayerType.value(player);
}

quint8 QAtemConnection::mediaPlayerSelectedStill(quint8 player) const
{
    return m_mediaPlayerSelectedStill.value(player);
}

quint8 QAtemConnection::mediaPlayerSelectedClip(quint8 player) const
{
    return m_mediaPlayerSelectedClip.value(player);
}

quint16 QAtemConnection::auxSource(quint8 aux) const
{
    return m_auxSource.value(aux);
}

void QAtemConnection::setMixFrames(quint8 frames)
{
    QByteArray cmd("CTMx");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)frames;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDipFrames(quint8 frames)
{
    QByteArray cmd("CTDp");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)frames;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDipSource(quint16 source)
{
    QByteArray cmd("CTDp");
    QByteArray payload(8, (char)0x0);
    U16_U8 val;

    payload[0] = (char)0x02;
    val.u16 = source;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeBorderSource(quint16 source)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;

    payload[1] = (char)0x08;
    val.u16 = source;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeFrames(quint8 frames)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x01;
    payload[3] = (char)frames;

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeBorderWidth(quint16 width)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = width;

    payload[1] = (char)0x04;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeBorderSoftness(quint16 softness)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = softness;

    payload[1] = (char)0x20;
    payload[12] = (char)val.u8[1];
    payload[13] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeType(quint8 type)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x02;
    payload[4] = (char)type;

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeSymmetry(quint16 value)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = value;

    payload[1] = (char)0x10;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeXPosition(quint16 value)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = value;

    payload[1] = (char)0x40;
    payload[14] = (char)val.u8[1];
    payload[15] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeYPosition(quint16 value)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = value;

    payload[1] = (char)0x80;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeReverseDirection(bool reverse)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x01;
    payload[18] = (char)reverse;

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeFlipFlop(bool flipFlop)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x02;
    payload[19] = (char)flipFlop;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVERate(quint16 frames)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = frames;

    payload[1] = (char)0x01;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEEffect(quint8 effect)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x04;
    payload[5] = (char)effect;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEFillSource(quint16 source)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;

    payload[1] = (char)0x08;
    val.u16 = source;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEKeySource(quint16 source)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;

    payload[1] = (char)0x10;
    val.u16 = source;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEKeyEnabled(bool enabled)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x20;
    payload[10] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEPreMultipliedKeyEnabled(bool enabled)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x40;
    payload[11] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEKeyClip(float percent)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = percent * 10;

    payload[1] = (char)0x80;
    payload[12] = (char)val.u8[1];
    payload[13] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEKeyGain(float percent)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = percent * 10;

    payload[0] = (char)0x01;
    payload[14] = (char)val.u8[1];
    payload[15] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEInvertKeyEnabled(bool enabled)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x02;
    payload[16] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEReverseDirection(bool reverse)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x04;
    payload[17] = (char)reverse;

    sendCommand(cmd, payload);
}

void QAtemConnection::setDVEFlipFlopDirection(bool flipFlop)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x08;
    payload[18] = (char)flipFlop;

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerSource(quint8 source)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x01;
    payload[3] = (char)source;

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerPreMultipliedKeyEnabled(bool enabled)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x02;
    payload[4] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerClip(float percent)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = percent * 10;

    payload[1] = (char)0x04;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerGain(float percent)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = percent * 10;

    payload[1] = (char)0x08;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerInvertKeyEnabled(bool enabled)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x10;
    payload[10] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerPreRoll(quint16 frames)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = frames;

    payload[1] = (char)0x20;
    payload[12] = (char)val.u8[1];
    payload[13] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerClipDuration(quint16 frames)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = frames;

    payload[1] = (char)0x40;
    payload[14] = (char)val.u8[1];
    payload[15] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerTriggerPoint(quint16 frames)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = frames;

    payload[1] = (char)0x80;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setStingerMixRate(quint16 frames)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    U16_U8 val;
    val.u16 = frames;

    payload[0] = (char)0x01;
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyType(quint8 keyer, quint8 type)
{
    if(type == m_upstreamKeys.value(keyer).m_type)
    {
        return;
    }

    QByteArray cmd("CKTp");
    QByteArray payload(10, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)keyer;
    payload[3] = (char)type;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyFillSource(quint8 keyer, quint16 source)
{
    if(source == m_upstreamKeys.value(keyer).m_fillSource)
    {
        return;
    }

    QByteArray cmd("CKeF");
    QByteArray payload(4, (char)0x0);
    U16_U8 val;

    payload[1] = (char)keyer;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyKeySource(quint8 keyer, quint16 source)
{
    if(source == m_upstreamKeys.value(keyer).m_keySource)
    {
        return;
    }

    QByteArray cmd("CKeC");
    QByteArray payload(4, (char)0x0);
    U16_U8 val;

    payload[1] = (char)keyer;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyEnableMask(quint8 keyer, bool enable)
{
    if(enable == m_upstreamKeys.value(keyer).m_enableMask)
    {
        return;
    }

    QByteArray cmd("CKMs");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)keyer;
    payload[3] = (char)enable;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyMask(quint8 keyer, float top, float bottom, float left, float right)
{
    QByteArray cmd("CKMs");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;

    payload[0] = (char)0x1e;
    payload[2] = (char)keyer;
    val.u16 = top * 1000;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];
    val.u16 = bottom * 1000;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];
    val.u16 = left * 1000;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];
    val.u16 = right * 1000;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLumaPreMultipliedKey(quint8 keyer, bool preMultiplied)
{
    if(preMultiplied == m_upstreamKeys.value(keyer).m_lumaPreMultipliedKey)
    {
        return;
    }

    QByteArray cmd("CKLm");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)keyer;
    payload[3] = (char)preMultiplied;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLumaInvertKey(quint8 keyer, bool invert)
{
    if(invert == m_upstreamKeys.value(keyer).m_lumaInvertKey)
    {
        return;
    }

    QByteArray cmd("CKLm");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x08;
    payload[2] = (char)keyer;
    payload[8] = (char)invert;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLumaClip(quint8 keyer, float clip)
{
    if(clip == m_upstreamKeys.value(keyer).m_lumaClip)
    {
        return;
    }

    QByteArray cmd("CKLm");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;
    val.u16 = clip * 10;

    payload[0] = (char)0x02;
    payload[2] = (char)keyer;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLumaGain(quint8 keyer, float gain)
{
    if(gain == m_upstreamKeys.value(keyer).m_lumaGain)
    {
        return;
    }

    QByteArray cmd("CKLm");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;
    val.u16 = gain * 10;

    payload[0] = (char)0x04;
    payload[2] = (char)keyer;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaHue(quint8 keyer, float hue)
{
    if(hue == m_upstreamKeys.value(keyer).m_chromaHue)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = hue * 10;

    payload[0] = (char)0x01;
    payload[2] = (char)keyer;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaGain(quint8 keyer, float gain)
{
    if(gain == m_upstreamKeys.value(keyer).m_chromaGain)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = gain * 10;

    payload[0] = (char)0x02;
    payload[2] = (char)keyer;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaYSuppress(quint8 keyer, float ySuppress)
{
    if(ySuppress == m_upstreamKeys.value(keyer).m_chromaYSuppress)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = ySuppress * 10;

    payload[0] = (char)0x04;
    payload[2] = (char)keyer;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaLift(quint8 keyer, float lift)
{
    if(lift == m_upstreamKeys.value(keyer).m_chromaLift)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = lift * 10;

    payload[0] = (char)0x08;
    payload[2] = (char)keyer;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaNarrowRange(quint8 keyer, bool narrowRange)
{
    if(narrowRange == m_upstreamKeys.value(keyer).m_chromaNarrowRange)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);

    payload[0] = (char)0x10;
    payload[2] = (char)keyer;
    payload[12] = (char)narrowRange;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternPattern(quint8 keyer, quint8 pattern)
{
    if(pattern == m_upstreamKeys.value(keyer).m_patternPattern)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)keyer;
    payload[3] = (char)pattern;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternInvertPattern(quint8 keyer, bool invert)
{
    if(invert == m_upstreamKeys.value(keyer).m_patternInvertPattern)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);

    payload[0] = (char)0x40;
    payload[2] = (char)keyer;
    payload[14] = (char)invert;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternSize(quint8 keyer, float size)
{
    if(size == m_upstreamKeys.value(keyer).m_patternSize)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = size * 100;

    payload[0] = (char)0x02;
    payload[2] = (char)keyer;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternSymmetry(quint8 keyer, float symmetry)
{
    if(symmetry == m_upstreamKeys.value(keyer).m_patternSymmetry)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = symmetry * 100;

    payload[0] = (char)0x04;
    payload[2] = (char)keyer;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternSoftness(quint8 keyer, float softness)
{
    if(softness == m_upstreamKeys.value(keyer).m_patternSoftness)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = softness * 100;

    payload[0] = (char)0x08;
    payload[2] = (char)keyer;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternXPosition(quint8 keyer, float xPosition)
{
    if(xPosition == m_upstreamKeys.value(keyer).m_patternXPosition)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = xPosition * 1000;

    payload[0] = (char)0x10;
    payload[2] = (char)keyer;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternYPosition(quint8 keyer, float yPosition)
{
    if(yPosition == m_upstreamKeys.value(keyer).m_patternYPosition)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    U16_U8 val;
    val.u16 = yPosition * 1000;

    payload[0] = (char)0x20;
    payload[2] = (char)keyer;
    payload[12] = (char)val.u8[1];
    payload[13] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEPosition(quint8 keyer, float xPosition, float yPosition)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    U16_U8 val;

    payload[3] = (char)0x0c;
    payload[5] = (char)keyer;
    val.u16 = xPosition * 1000;
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];
    val.u16 = yPosition * 1000;
    payload[22] = (char)val.u8[1];
    payload[23] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVESize(quint8 keyer, float xSize, float ySize)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    U16_U8 val;

    payload[3] = (char)0x03;
    payload[5] = (char)keyer;
    val.u16 = xSize * 1000;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];
    val.u16 = ySize * 1000;
    payload[14] = (char)val.u8[1];
    payload[15] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVERotation(quint8 keyer, float rotation)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    U16_U8 val;

    payload[3] = (char)0x10;
    payload[5] = (char)keyer;
    val.u16 = rotation * 10;
    payload[26] = (char)val.u8[1];
    payload[27] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVELightSource(quint8 keyer, float direction, quint8 altitude)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    U16_U8 val;

    payload[1] = (char)0x0c;
    payload[5] = (char)keyer;
    val.u16 = direction * 10;
    payload[48] = (char)val.u8[1];
    payload[49] = (char)val.u8[0];
    payload[50] = (char)altitude;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEDropShadowEnabled(quint8 keyer, bool enabled)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[3] = (char)0x40;
    payload[5] = (char)keyer;
    payload[29] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderEnabled(quint8 keyer, bool enabled)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[3] = (char)0x20;
    payload[5] = (char)keyer;
    payload[28] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderStyle(quint8 keyer, quint8 style)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[3] = (char)0x80;
    payload[5] = (char)keyer;
    payload[30] = (char)style;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderColorH(quint8 keyer, float h)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    U16_U8 val;

    payload[2] = (char)0x80;
    payload[5] = (char)keyer;
    val.u16 = h * 10;
    payload[42] = (char)val.u8[1];
    payload[43] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderColorS(quint8 keyer, float s)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    U16_U8 val;

    payload[1] = (char)0x01;
    payload[5] = (char)keyer;
    val.u16 = s * 10;
    payload[44] = (char)val.u8[1];
    payload[45] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderColorL(quint8 keyer, float l)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    U16_U8 val;

    payload[1] = (char)0x02;
    payload[5] = (char)keyer;
    val.u16 = l * 10;
    payload[46] = (char)val.u8[1];
    payload[47] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderColor(quint8 keyer, const QColor& color)
{
    setUpstreamKeyDVEBorderColorH(keyer, qMax((qreal)0.0, color.hslHueF()) * 360.0);
    setUpstreamKeyDVEBorderColorS(keyer, color.hslSaturationF() * 100);
    setUpstreamKeyDVEBorderColorL(keyer, color.lightnessF() * 100);
}

void QAtemConnection::setUpstreamKeyDVEBorderWidth(quint8 keyer, float outside, float inside)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    U16_U8 val;

    payload[2] = (char)0x03;
    payload[5] = (char)keyer;
    val.u16 = outside * 100;
    payload[32] = (char)val.u8[1];
    payload[33] = (char)val.u8[0];
    val.u16 = inside * 100;
    payload[34] = (char)val.u8[1];
    payload[35] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderSoften(quint8 keyer, quint8 outside, quint8 inside)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[2] = (char)0x0c;
    payload[5] = (char)keyer;
    payload[36] = (char)outside;
    payload[37] = (char)inside;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderOpacity(quint8 keyer, quint8 opacity)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[2] = (char)0x40;
    payload[5] = (char)keyer;
    payload[40] = (char)opacity;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderBevelPosition(quint8 keyer, float position)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[2] = (char)0x20;
    payload[5] = (char)keyer;
    payload[39] = (char)(position * 100);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEBorderBevelSoften(quint8 keyer, quint8 soften)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[2] = (char)0x10;
    payload[5] = (char)keyer;
    payload[38] = (char)soften;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVERate(quint8 keyer, quint8 rate)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[0] = (char)0x02;
    payload[5] = (char)keyer;
    payload[60] = (char)rate;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyDVEKeyFrame(quint8 keyer, quint8 keyFrame)
{
    QByteArray cmd("SFKF");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)keyer;
    payload[2] = (char)keyFrame;

    sendCommand(cmd, payload);
}

void QAtemConnection::runUpstreamKeyTo(quint8 keyer, quint8 position, quint8 direction)
{
    QByteArray cmd("RFlK");
    QByteArray payload(8, (char)0x0);

    payload[2] = (char)keyer;

    if(position == 4)
    {
        payload[0] = (char)0x02; // This is needed else the direction will be ignore
        payload[4] = (char)direction;
    }

    payload[5] = (char)position;

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyFlyEnabled(quint8 keyer, bool enable)
{
    QByteArray cmd("CKTp");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x02;
    payload[2] = (char)keyer;
    payload[4] = (char)enable;

    sendCommand(cmd, payload);
}

void QAtemConnection::setAuxSource(quint8 aux, quint16 source)
{
    if(source == m_auxSource.value(aux))
    {
        return;
    }

    QByteArray cmd("CAuS");
    QByteArray payload(8, (char)0x0);
    U16_U8 val;

    payload[0] = 0x01;
    payload[1] = (char)aux;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setInputType(quint16 input, quint8 type)
{
    if(type == m_inputInfos.value(input).type)
    {
        return;
    }

    QByteArray cmd("CInL");
    QByteArray payload(32, (char)0x0);
    U16_U8 val;

    payload[0] = (char)0x04;
    val.u16 = input;
    payload[1] = (char)val.u8[1];
    payload[2] = (char)val.u8[0];
    payload[29] = (char)type;

    sendCommand(cmd, payload);
}

void QAtemConnection::setInputLongName(quint16 input, const QString &name)
{
    if(name == m_inputInfos.value(input).longText)
    {
        return;
    }

    QByteArray cmd("CInL");
    QByteArray payload(32, (char)0x0);
    QByteArray namearray = name.toLatin1();
    namearray.resize(20);
    U16_U8 val;

    payload[0] = (char)0x01;
    val.u16 = input;
    payload[1] = (char)val.u8[1];
    payload[2] = (char)val.u8[0];
    payload.replace(3, 21, namearray);
    payload[28] = (char)0xff;

    sendCommand(cmd, payload);
}

void QAtemConnection::setInputShortName(quint16 input, const QString &name)
{
    if(name == m_inputInfos.value(input).shortText)
    {
        return;
    }

    QByteArray cmd("CInL");
    QByteArray payload(32, (char)0x0);
    QByteArray namearray = name.toLatin1();
    namearray.resize(4);
    U16_U8 val;

    payload[0] = (char)0x02;
    val.u16 = input;
    payload[1] = (char)val.u8[1];
    payload[2] = (char)val.u8[0];
    payload.replace(24, 4, namearray);

    sendCommand(cmd, payload);
}

void QAtemConnection::setVideoFormat(quint8 format)
{
    if(format == m_videoFormat)
    {
        return;
    }

    QByteArray cmd("CVdM");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)format;

    sendCommand(cmd, payload);
}

void QAtemConnection::setVideoDownConvertType(quint8 type)
{
    if (type == m_videoDownConvertType)
    {
        return;
    }

    QByteArray cmd("CDcO");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)type;

    sendCommand(cmd, payload);
}

void QAtemConnection::setMediaPoolClipSplit(quint8 size)
{
    QByteArray cmd("CMPS");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)size;

    sendCommand(cmd, payload);
}

void QAtemConnection::setMultiViewLayout(quint8 layout)
{
    if(layout == m_multiViewLayout)
    {
        return;
    }

    QByteArray cmd("CMvP");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)layout;

    sendCommand(cmd, payload);
}

void QAtemConnection::onPrgI(const QByteArray& payload)
{
    quint16 old = m_programInput;
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_programInput = val.u16;
    emit programInputChanged(old, m_programInput);
}

void QAtemConnection::onPrvI(const QByteArray& payload)
{
    quint16 old = m_previewInput;
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_previewInput = val.u16;
    emit previewInputChanged(old, m_previewInput);
}

void QAtemConnection::onTlIn(const QByteArray& payload)
{
    m_tallyStateCount = payload.at(7);

    for(quint8 i = 0; i < m_tallyStateCount; ++i)
    {
        m_tallyStates[i] = (quint8)payload.at(8 + i);
    }

    emit tallyStatesChanged();
}

void QAtemConnection::onTrPr(const QByteArray& payload)
{
    m_transitionPreviewEnabled = (payload.at(7) > 0);

    emit transitionPreviewChanged(m_transitionPreviewEnabled);
}

void QAtemConnection::onTrPs(const QByteArray& payload)
{
    m_transitionFrameCount = (quint8)payload.at(8);
    m_transitionPosition = ((quint8)payload.at(11) | ((quint8)payload.at(10) << 8));

    emit transitionFrameCountChanged(m_transitionFrameCount);
    emit transitionPositionChanged(m_transitionPosition);
}

void QAtemConnection::onTrSS(const QByteArray& payload)
{
    m_currentTransitionStyle = (quint8)payload.at(7); // Bit 0 = Mix, 1 = Dip, 2 = Wipe, 3 = DVE and 4 = Stinger, only bit 0-2 available on TVS
    m_keyersOnCurrentTransition = ((quint8)payload.at(8) & 0x1f); // Bit 0 = Background, 1-4 = keys, only bit 0 and 1 available on TVS
    m_nextTransitionStyle = (quint8)payload.at(9); // Bit 0 = Mix, 1 = Dip, 2 = Wipe, 3 = DVE and 4 = Stinger, only bit 0-2 available on TVS
    m_keyersOnNextTransition = ((quint8)payload.at(10) & 0x1f); // Bit 0 = Background, 1-4 = keys, only bit 0 and 1 available on TVS

    emit nextTransitionStyleChanged(m_nextTransitionStyle);
    emit keyersOnNextTransitionChanged(m_keyersOnNextTransition);
    emit currentTransitionStyleChanged(m_currentTransitionStyle);
    emit keyersOnCurrentTransitionChanged(m_keyersOnCurrentTransition);
}

void QAtemConnection::onFtbS(const QByteArray& payload)
{
    m_fadeToBlackEnabled = (bool)payload.at(7);
    m_fadeToBlackFading = (bool)payload.at(8);
    m_fadeToBlackFrameCount = (quint8)payload.at(9);

    emit fadeToBlackChanged(m_fadeToBlackFading, m_fadeToBlackEnabled);
    emit fadeToBlackFrameCountChanged(m_fadeToBlackFrameCount);
}

void QAtemConnection::onFtbP(const QByteArray& payload)
{
    m_fadeToBlackFrames = (quint8)payload.at(7);

    emit fadeToBlackFramesChanged(m_fadeToBlackFrames);
}

void QAtemConnection::onDskS(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);
    m_downstreamKeys[index].m_onAir = (quint8)payload.at(7);
    m_downstreamKeys[index].m_frameCount = (quint8)payload.at(10);

    emit downstreamKeyOnChanged(index, m_downstreamKeys[index].m_onAir);
    emit downstreamKeyFrameCountChanged(index, m_downstreamKeys[index].m_frameCount);
}

void QAtemConnection::onDskP(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);
    m_downstreamKeys[index].m_tie = (quint8)payload.at(7);
    m_downstreamKeys[index].m_frames = (quint8)payload.at(8);
    m_downstreamKeys[index].m_preMultiplied = (quint8)payload.at(9);
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(10);
    val.u8[0] = (quint8)payload.at(11);
    m_downstreamKeys[index].m_clip = val.u16 / 10.0;
    val.u8[1] = (quint8)payload.at(12);
    val.u8[0] = (quint8)payload.at(13);
    m_downstreamKeys[index].m_gain = val.u16 / 10.0;
    m_downstreamKeys[index].m_invertKey = (quint8)payload.at(14);
    m_downstreamKeys[index].m_enableMask = (quint8)payload.at(15);
    val.u8[1] = (quint8)payload.at(16);
    val.u8[0] = (quint8)payload.at(17);
    m_downstreamKeys[index].m_topMask = (qint16)val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(18);
    val.u8[0] = (quint8)payload.at(19);
    m_downstreamKeys[index].m_bottomMask = (qint16)val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(20);
    val.u8[0] = (quint8)payload.at(21);
    m_downstreamKeys[index].m_leftMask = (qint16)val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(22);
    val.u8[0] = (quint8)payload.at(23);
    m_downstreamKeys[index].m_rightMask = (qint16)val.u16 / 1000.0;

    emit downstreamKeyTieChanged(index, m_downstreamKeys[index].m_tie);
    emit downstreamKeyFramesChanged(index, m_downstreamKeys[index].m_frames);
    emit downstreamKeyInvertKeyChanged(index, m_downstreamKeys[index].m_invertKey);
    emit downstreamKeyPreMultipliedChanged(index, m_downstreamKeys[index].m_preMultiplied);
    emit downstreamKeyClipChanged(index, m_downstreamKeys[index].m_clip);
    emit downstreamKeyGainChanged(index, m_downstreamKeys[index].m_gain);
    emit downstreamKeyEnableMaskChanged(index, m_downstreamKeys[index].m_enableMask);
    emit downstreamKeyTopMaskChanged(index, m_downstreamKeys[index].m_topMask);
    emit downstreamKeyBottomMaskChanged(index, m_downstreamKeys[index].m_bottomMask);
    emit downstreamKeyLeftMaskChanged(index, m_downstreamKeys[index].m_leftMask);
    emit downstreamKeyRightMaskChanged(index, m_downstreamKeys[index].m_rightMask);
}

void QAtemConnection::onDskB(const QByteArray& payload)
{
    U16_U8 val;
    quint8 index = (quint8)payload.at(6);
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_downstreamKeys[index].m_fillSource = val.u16;
    val.u8[1] = (quint8)payload.at(10);
    val.u8[0] = (quint8)payload.at(11);
    m_downstreamKeys[index].m_keySource = val.u16;

    emit downstreamKeySourcesChanged(index, m_downstreamKeys[index].m_fillSource, m_downstreamKeys[index].m_keySource);
}

void QAtemConnection::onKeOn(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(7);
    m_upstreamKeys[index].m_onAir = (quint8)payload.at(8);

    emit upstreamKeyOnChanged(index, m_upstreamKeys[index].m_onAir);
}

void QAtemConnection::onColV(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);

    U16_U8 h, s, l;

    h.u8[1] = (quint8)payload.at(8);
    h.u8[0] = (quint8)payload.at(9);
    s.u8[1] = (quint8)payload.at(10);
    s.u8[0] = (quint8)payload.at(11);
    l.u8[1] = (quint8)payload.at(12);
    l.u8[0] = (quint8)payload.at(13);

    QColor color;
    float hf = ((h.u16 / 10) % 360) / 360.0;
    color.setHslF(hf, s.u16 / 1000.0, l.u16 / 1000.0);
    m_colorGeneratorColors[index] = color;

    emit colorGeneratorColorChanged(index, m_colorGeneratorColors[index]);
}

void QAtemConnection::onMPCE(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);

    m_mediaPlayerType[index] = (quint8)payload.at(7);
    m_mediaPlayerSelectedStill[index] = (quint8)payload.at(8);
    m_mediaPlayerSelectedClip[index] = (quint8)payload.at(9);

    emit mediaPlayerChanged(index, m_mediaPlayerType[index], m_mediaPlayerSelectedStill[index], m_mediaPlayerSelectedClip[index]);
}

void QAtemConnection::onAuxS(const QByteArray& payload)
{
    U16_U8 val;
    quint8 index = (quint8)payload.at(6);

    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_auxSource[index] = val.u16;

    emit auxSourceChanged(index, m_auxSource[index]);
}

void QAtemConnection::on_pin(const QByteArray& payload)
{
    m_productInformation = payload.mid(6);

    emit productInformationChanged(m_productInformation);
}

void QAtemConnection::on_ver(const QByteArray& payload)
{
    U16_U8 ver;
    ver.u8[1] = payload.at(6);
    ver.u8[0] = payload.at(7);
    m_majorversion = ver.u16;
    ver.u8[1] = payload.at(8);
    ver.u8[0] = payload.at(9);
    m_minorversion = ver.u16;

    emit versionChanged(m_majorversion, m_minorversion);
}

void QAtemConnection::onInPr(const QByteArray& payload)
{
    InputInfo info;
    U16_U8 index;
    index.u8[1] = (quint8)payload.at(6);
    index.u8[0] = (quint8)payload.at(7);
    info.index = index.u16;
    info.longText = payload.mid(8, 20);
    info.shortText = payload.mid(28, 4);
    info.type = (quint8)payload.at(36); // 1 = SDI, 2 = HDMI, 4 = Component, 0 = Internal
    m_inputInfos.insert(info.index, info);

    emit inputInfoChanged(info);
}

void QAtemConnection::onMPSE(const QByteArray& payload)
{
    MediaInfo info;
    info.index = (quint8)payload.at(6);
    info.used = (quint8)payload.at(7);

    if(info.used)
    {
        info.name = payload.mid(8);
    }

    m_stillMediaInfos.insert(info.index, info);

    emit mediaInfoChanged(info);
}

void QAtemConnection::onMPfe(const QByteArray& payload)
{
    MediaInfo info;
    info.type = StillMedia;
    info.index = (quint8)payload.at(9);
    info.used = (quint8)payload.at(10);
    info.hash = payload.mid(11, 16);
    quint8 length = (quint8)payload.at(29);

    if(info.used)
    {
        info.name = payload.mid(30, length);
    }

    m_stillMediaInfos.insert(info.index, info);

    emit mediaInfoChanged(info);
}

void QAtemConnection::onMPCS(const QByteArray& payload)
{
    MediaInfo info;
    info.type = ClipMedia;
    info.index = (quint8)payload.at(6);
    info.used = (quint8)payload.at(7);

    if(info.used)
    {
        info.name = payload.mid(8);
    }

    m_clipMediaInfos.insert(info.index, info);

    emit mediaInfoChanged(info);
}

void QAtemConnection::onMvIn(const QByteArray& payload)
{
    quint8 mvindex = (quint8)payload.at(7);    // Index of multiview output
    quint8 inputindex = (quint8)payload.at(8); // Index of input, these are mapped in the InPr command
    m_multiViewInputs[mvindex] = inputindex;
}

void QAtemConnection::onMvPr(const QByteArray& payload)
{
    m_multiViewLayout = (quint8)payload.at(7);
}

void QAtemConnection::onVidM(const QByteArray& payload)
{
    m_videoFormat = (quint8)payload.at(6); // 0 = 525i5994, 1 = 625i50, 2 = 525i5994 16:9, 3 = 625i50 16:9, 4 = 720p50, 5 = 720p5994, 6 = 1080i50, 7 = 1080i5994

    switch(m_videoFormat)
    {
    case 0:
    case 2:
    case 7:
        m_framesPerSecond = 30;
        break;
    case 5:
        m_framesPerSecond = 60;
        break;
    case 1:
    case 3:
    case 6:
        m_framesPerSecond = 25;
        break;
    case 4:
        m_framesPerSecond = 50;
        break;
    default:
        m_framesPerSecond = 0;
        break;
    }

    emit videoFormatChanged(m_videoFormat);
}

void QAtemConnection::onTime(const QByteArray& payload)
{
    U32_U8 val;
    val.u8[3] = (quint8)payload.at(6);
    val.u8[2] = (quint8)payload.at(7);
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);

    emit timeChanged(val.u32);
}

void QAtemConnection::onTMxP(const QByteArray& payload)
{
    m_mixFrames = (quint8)payload.at(7);

    emit mixFramesChanged(m_mixFrames);
}

void QAtemConnection::onTDpP(const QByteArray& payload)
{
    U16_U8 val;
    m_dipFrames = (quint8)payload.at(7);
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_dipSource = val.u16;

    emit dipFramesChanged(m_dipFrames);
    emit dipSourceChanged(m_dipSource);
}

void QAtemConnection::onTWpP(const QByteArray& payload)
{
    m_wipeFrames = (quint8)payload.at(7);
    m_wipeType = (quint8)payload.at(8);

    U16_U8 val;
    val.u8[1] = (quint8)payload.at(10);
    val.u8[0] = (quint8)payload.at(11);
    m_wipeBorderWidth = val.u16;
    val.u8[1] = (quint8)payload.at(12);
    val.u8[0] = (quint8)payload.at(13);
    m_wipeBorderSource = val.u16;
    val.u8[1] = (quint8)payload.at(14);
    val.u8[0] = (quint8)payload.at(15);
    m_wipeSymmetry = val.u16;
    val.u8[1] = (quint8)payload.at(16);
    val.u8[0] = (quint8)payload.at(17);
    m_wipeBorderSoftness = val.u16;
    val.u8[1] = (quint8)payload.at(18);
    val.u8[0] = (quint8)payload.at(19);
    m_wipeXPosition = val.u16;
    val.u8[1] = (quint8)payload.at(20);
    val.u8[0] = (quint8)payload.at(21);
    m_wipeYPosition = val.u16;
    m_wipeReverseDirection = (quint8)payload.at(22);
    m_wipeFlipFlop = (quint8)payload.at(23);

    emit wipeFramesChanged(m_wipeFrames);
    emit wipeBorderWidthChanged(m_wipeBorderWidth);
    emit wipeBorderSourceChanged(m_wipeBorderSource);
    emit wipeBorderSoftnessChanged(m_wipeBorderSoftness);
    emit wipeTypeChanged(m_wipeType);
    emit wipeSymmetryChanged(m_wipeSymmetry);
    emit wipeXPositionChanged(m_wipeXPosition);
    emit wipeYPositionChanged(m_wipeYPosition);
    emit wipeReverseDirectionChanged(m_wipeReverseDirection);
    emit wipeFlipFlopChanged(m_wipeFlipFlop);
}

void QAtemConnection::onTDvP(const QByteArray& payload)
{
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);
    m_dveRate = val.u16;
    m_dveEffect = (quint8)payload.at(9);
    val.u8[1] = (quint8)payload.at(10);
    val.u8[0] = (quint8)payload.at(11);
    m_dveFillSource = val.u16;
    val.u8[1] = (quint8)payload.at(12);
    val.u8[0] = (quint8)payload.at(13);
    m_dveKeySource = val.u16;
    m_dveEnableKey = (bool)payload.at(14);
    m_dveEnablePreMultipliedKey = (bool)payload.at(15);
    val.u8[1] = (quint8)payload.at(16);
    val.u8[0] = (quint8)payload.at(17);
    m_dveKeyClip = val.u16 / 10.0;
    val.u8[1] = (quint8)payload.at(18);
    val.u8[0] = (quint8)payload.at(19);
    m_dveKeyGain = val.u16 / 10.0;
    m_dveEnableInvertKey = (bool)payload.at(20);
    m_dveReverseDirection = (bool)payload.at(21);
    m_dveFlipFlopDirection = (bool)payload.at(22);

    emit dveRateChanged(m_dveRate);
    emit dveEffectChanged(m_dveEffect);
    emit dveFillSourceChanged(m_dveFillSource);
    emit dveKeySourceChanged(m_dveKeySource);
    emit dveEnableKeyChanged(m_dveEnableKey);
    emit dveEnablePreMultipliedKeyChanged(m_dveEnablePreMultipliedKey);
    emit dveKeyClipChanged(m_dveKeyClip);
    emit dveKeyGainChanged(m_dveKeyGain);
    emit dveEnableInvertKeyChanged(m_dveEnableInvertKey);
    emit dveReverseDirectionChanged(m_dveReverseDirection);
    emit dveFlipFlopDirectionChanged(m_dveFlipFlopDirection);
}

void QAtemConnection::onTStP(const QByteArray& payload)
{
    m_stingerSource = (quint8)payload.at(7);
    m_stingerEnablePreMultipliedKey = (quint8)payload.at(8);
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(10);
    val.u8[0] = (quint8)payload.at(11);
    m_stingerClip = val.u16 / 10.0;
    val.u8[1] = (quint8)payload.at(12);
    val.u8[0] = (quint8)payload.at(13);
    m_stingerGain = val.u16 / 10.0;
    m_stingerEnableInvertKey = (bool)payload.at(14);
    val.u8[1] = (quint8)payload.at(16);
    val.u8[0] = (quint8)payload.at(17);
    m_stingerPreRoll = val.u16;
    val.u8[1] = (quint8)payload.at(18);
    val.u8[0] = (quint8)payload.at(19);
    m_stingerClipDuration = val.u16;
    val.u8[1] = (quint8)payload.at(20);
    val.u8[0] = (quint8)payload.at(21);
    m_stingerTriggerPoint = val.u16;
    val.u8[1] = (quint8)payload.at(22);
    val.u8[0] = (quint8)payload.at(23);
    m_stingerMixRate = val.u16;

    emit stingerSourceChanged(m_stingerSource);
    emit stingerEnablePreMultipliedKeyChanged(m_stingerEnablePreMultipliedKey);
    emit stingerClipChanged(m_stingerClip);
    emit stingerGainChanged(m_stingerGain);
    emit stingerEnableInvertKeyChanged(m_stingerEnableInvertKey);
    emit stingerPreRollChanged(m_stingerPreRoll);
    emit stingerClipDurationChanged(m_stingerClipDuration);
    emit stingerTriggerPointChanged(m_stingerTriggerPoint);
    emit stingerMixRateChanged(m_stingerMixRate);
}

void QAtemConnection::onKeBP(const QByteArray& payload)
{
    U16_U8 val;
    quint8 index = (quint8)payload.at(7);
    m_upstreamKeys[index].m_type = (quint8)payload.at(8);
    m_upstreamKeys[index].m_enableFly = (bool)payload.at(11);
    val.u8[1] = (quint8)payload[12];
    val.u8[0] = (quint8)payload[13];
    m_upstreamKeys[index].m_fillSource = val.u16;
    val.u8[1] = (quint8)payload[14];
    val.u8[0] = (quint8)payload[15];
    m_upstreamKeys[index].m_keySource = val.u16;
    m_upstreamKeys[index].m_enableMask = (quint8)payload.at(16);
    val.u8[1] = (quint8)payload.at(18);
    val.u8[0] = (quint8)payload.at(19);
    m_upstreamKeys[index].m_topMask = (qint16)val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(20);
    val.u8[0] = (quint8)payload.at(21);
    m_upstreamKeys[index].m_bottomMask = (qint16)val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(22);
    val.u8[0] = (quint8)payload.at(23);
    m_upstreamKeys[index].m_leftMask = (qint16)val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(24);
    val.u8[0] = (quint8)payload.at(25);
    m_upstreamKeys[index].m_rightMask = (qint16)val.u16 / 1000.0;

    emit upstreamKeyTypeChanged(index, m_upstreamKeys[index].m_type);
    emit upstreamKeyEnableFlyChanged(index, m_upstreamKeys[index].m_enableFly);
    emit upstreamKeyFillSourceChanged(index, m_upstreamKeys[index].m_fillSource);
    emit upstreamKeyKeySourceChanged(index, m_upstreamKeys[index].m_keySource);
    emit upstreamKeyEnableMaskChanged(index, m_upstreamKeys[index].m_enableMask);
    emit upstreamKeyTopMaskChanged(index, m_upstreamKeys[index].m_topMask);
    emit upstreamKeyBottomMaskChanged(index, m_upstreamKeys[index].m_bottomMask);
    emit upstreamKeyLeftMaskChanged(index, m_upstreamKeys[index].m_leftMask);
    emit upstreamKeyRightMaskChanged(index, m_upstreamKeys[index].m_rightMask);
}

void QAtemConnection::onKeLm(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(7);
    m_upstreamKeys[index].m_lumaPreMultipliedKey = (quint8)payload.at(8);
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(10);
    val.u8[0] = (quint8)payload.at(11);
    m_upstreamKeys[index].m_lumaClip = val.u16 / 10.0;
    val.u8[1] = (quint8)payload.at(12);
    val.u8[0] = (quint8)payload.at(13);
    m_upstreamKeys[index].m_lumaGain = val.u16 / 10.0;
    m_upstreamKeys[index].m_lumaInvertKey = (quint8)payload.at(14);

    emit upstreamKeyLumaPreMultipliedKeyChanged(index, m_upstreamKeys[index].m_lumaPreMultipliedKey);
    emit upstreamKeyLumaClipChanged(index, m_upstreamKeys[index].m_lumaClip);
    emit upstreamKeyLumaGainChanged(index, m_upstreamKeys[index].m_lumaGain);
    emit upstreamKeyLumaInvertKeyChanged(index, m_upstreamKeys[index].m_lumaInvertKey);
}

void QAtemConnection::onKeCk(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(7);
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_upstreamKeys[index].m_chromaHue = val.u16 / 10.0;
    val.u8[1] = (quint8)payload.at(10);
    val.u8[0] = (quint8)payload.at(11);
    m_upstreamKeys[index].m_chromaGain = val.u16 / 10.0;
    val.u8[1] = (quint8)payload.at(12);
    val.u8[0] = (quint8)payload.at(13);
    m_upstreamKeys[index].m_chromaYSuppress = val.u16 / 10.0;
    val.u8[1] = (quint8)payload.at(14);
    val.u8[0] = (quint8)payload.at(15);
    m_upstreamKeys[index].m_chromaLift = val.u16 / 10.0;
    m_upstreamKeys[index].m_chromaNarrowRange = (quint8)payload.at(16);

    emit upstreamKeyChromaHueChanged(index, m_upstreamKeys[index].m_chromaHue);
    emit upstreamKeyChromaGainChanged(index, m_upstreamKeys[index].m_chromaGain);
    emit upstreamKeyChromaYSuppressChanged(index, m_upstreamKeys[index].m_chromaYSuppress);
    emit upstreamKeyChromaLiftChanged(index, m_upstreamKeys[index].m_chromaLift);
    emit upstreamKeyChromaNarrowRangeChanged(index, m_upstreamKeys[index].m_chromaNarrowRange);
}

void QAtemConnection::onKePt(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(7);
    m_upstreamKeys[index].m_patternPattern = (quint8)payload.at(8);
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(10);
    val.u8[0] = (quint8)payload.at(11);
    m_upstreamKeys[index].m_patternSize = val.u16 / 100.0;
    val.u8[1] = (quint8)payload.at(12);
    val.u8[0] = (quint8)payload.at(13);
    m_upstreamKeys[index].m_patternSymmetry = val.u16 / 100.0;
    val.u8[1] = (quint8)payload.at(14);
    val.u8[0] = (quint8)payload.at(15);
    m_upstreamKeys[index].m_patternSoftness = val.u16 / 100.0;
    val.u8[1] = (quint8)payload.at(16);
    val.u8[0] = (quint8)payload.at(17);
    m_upstreamKeys[index].m_patternXPosition = val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(18);
    val.u8[0] = (quint8)payload.at(19);
    m_upstreamKeys[index].m_patternYPosition = val.u16 / 1000.0;
    m_upstreamKeys[index].m_patternInvertPattern = (quint8)payload.at(20);

    emit upstreamKeyPatternPatternChanged(index, m_upstreamKeys[index].m_patternPattern);
    emit upstreamKeyPatternSizeChanged(index, m_upstreamKeys[index].m_patternSize);
    emit upstreamKeyPatternSymmetryChanged(index, m_upstreamKeys[index].m_patternSymmetry);
    emit upstreamKeyPatternSoftnessChanged(index, m_upstreamKeys[index].m_patternSoftness);
    emit upstreamKeyPatternXPositionChanged(index, m_upstreamKeys[index].m_patternXPosition);
    emit upstreamKeyPatternYPositionChanged(index, m_upstreamKeys[index].m_patternYPosition);
    emit upstreamKeyPatternInvertPatternChanged(index, m_upstreamKeys[index].m_patternInvertPattern);
}

void QAtemConnection::onKeDV(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(7);
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(12);
    val.u8[0] = (quint8)payload.at(13);
    m_upstreamKeys[index].m_dveXSize = val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(16);
    val.u8[0] = (quint8)payload.at(17);
    m_upstreamKeys[index].m_dveYSize = val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(20);
    val.u8[0] = (quint8)payload.at(21);
    m_upstreamKeys[index].m_dveXPosition = val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(24);
    val.u8[0] = (quint8)payload.at(25);
    m_upstreamKeys[index].m_dveYPosition = val.u16 / 1000.0;
    val.u8[1] = (quint8)payload.at(28);
    val.u8[0] = (quint8)payload.at(29);
    m_upstreamKeys[index].m_dveRotation = val.u16 / 10.0;
    m_upstreamKeys[index].m_dveEnableBorder = (bool)payload.at(30);
    m_upstreamKeys[index].m_dveEnableDropShadow = (bool)payload.at(31);
    m_upstreamKeys[index].m_dveBorderStyle = (quint8)payload.at(32);
    val.u8[1] = (quint8)payload.at(34);
    val.u8[0] = (quint8)payload.at(35);
    m_upstreamKeys[index].m_dveBorderOutsideWidth = val.u16 / 100.0;
    val.u8[1] = (quint8)payload.at(36);
    val.u8[0] = (quint8)payload.at(37);
    m_upstreamKeys[index].m_dveBorderInsideWidth = val.u16 / 100.0;
    m_upstreamKeys[index].m_dveBorderOutsideSoften = (quint8)payload.at(38);
    m_upstreamKeys[index].m_dveBorderInsideSoften = (quint8)payload.at(39);
    m_upstreamKeys[index].m_dveBorderBevelSoften = (quint8)payload.at(40);
    m_upstreamKeys[index].m_dveBorderBevelPosition = ((quint8)payload.at(41)) / 100.0;
    m_upstreamKeys[index].m_dveBorderOpacity = (quint8)payload.at(42);
    U16_U8 h, s, l;

    h.u8[1] = (quint8)payload.at(44);
    h.u8[0] = (quint8)payload.at(45);
    s.u8[1] = (quint8)payload.at(46);
    s.u8[0] = (quint8)payload.at(47);
    l.u8[1] = (quint8)payload.at(48);
    l.u8[0] = (quint8)payload.at(49);

    QColor color;
    float hf = ((h.u16 / 10) % 360) / 360.0;
    color.setHslF(hf, s.u16 / 1000.0, l.u16 / 1000.0);
    m_upstreamKeys[index].m_dveBorderColor = color;
    val.u8[1] = (quint8)payload.at(50);
    val.u8[0] = (quint8)payload.at(51);
    m_upstreamKeys[index].m_dveLightSourceDirection = val.u16 / 10.0;
    m_upstreamKeys[index].m_dveLightSourceAltitude = (quint8)payload.at(52);
    m_upstreamKeys[index].m_dveRate = (quint8)payload.at(62);

    emit upstreamKeyDVEXPositionChanged(index, m_upstreamKeys[index].m_dveXPosition);
    emit upstreamKeyDVEYPositionChanged(index, m_upstreamKeys[index].m_dveYPosition);
    emit upstreamKeyDVEXSizeChanged(index, m_upstreamKeys[index].m_dveXSize);
    emit upstreamKeyDVEYSizeChanged(index, m_upstreamKeys[index].m_dveYSize);
    emit upstreamKeyDVERotationChanged(index, m_upstreamKeys[index].m_dveRotation);
    emit upstreamKeyDVEEnableDropShadowChanged(index, m_upstreamKeys[index].m_dveEnableDropShadow);
    emit upstreamKeyDVELighSourceDirectionChanged(index, m_upstreamKeys[index].m_dveLightSourceDirection);
    emit upstreamKeyDVELightSourceAltitudeChanged(index, m_upstreamKeys[index].m_dveLightSourceAltitude);
    emit upstreamKeyDVEEnableBorderChanged(index, m_upstreamKeys[index].m_dveEnableBorder);
    emit upstreamKeyDVEBorderStyleChanged(index, m_upstreamKeys[index].m_dveBorderStyle);
    emit upstreamKeyDVEBorderColorChanged(index, m_upstreamKeys[index].m_dveBorderColor);
    emit upstreamKeyDVEBorderOutsideWidthChanged(index, m_upstreamKeys[index].m_dveBorderOutsideWidth);
    emit upstreamKeyDVEBorderInsideWidthChanged(index, m_upstreamKeys[index].m_dveBorderInsideWidth);
    emit upstreamKeyDVEBorderOutsideSoftenChanged(index, m_upstreamKeys[index].m_dveBorderOutsideSoften);
    emit upstreamKeyDVEBorderInsideSoftenChanged(index, m_upstreamKeys[index].m_dveBorderInsideSoften);
    emit upstreamKeyDVEBorderOpacityChanged(index, m_upstreamKeys[index].m_dveBorderOpacity);
    emit upstreamKeyDVERateChanged(index, m_upstreamKeys[index].m_dveRate);
}

void QAtemConnection::onKeFS(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(7);
    m_upstreamKeys[index].m_dveKeyFrameASet = (bool)payload.at(8);
    m_upstreamKeys[index].m_dveKeyFrameBSet = (bool)payload.at(9);

    emit upstreamKeyDVEKeyFrameASetChanged(index, m_upstreamKeys[index].m_dveKeyFrameASet);
    emit upstreamKeyDVEKeyFrameBSetChanged(index, m_upstreamKeys[index].m_dveKeyFrameBSet);
}

void QAtemConnection::onDcOt(const QByteArray& payload)
{
    m_videoDownConvertType = (quint8)payload.at(6);

    emit videoDownConvertTypeChanged(m_videoDownConvertType);
}

void QAtemConnection::onMPSp(const QByteArray& payload)
{
    m_mediaPoolClip1Size = (quint8)payload.at(7);
    m_mediaPoolClip2Size = (quint8)payload.at(9);

    emit mediaPoolClip1SizeChanged(m_mediaPoolClip1Size);
    emit mediaPoolClip2SizeChanged(m_mediaPoolClip2Size);
}

void QAtemConnection::onRCPS(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);
    m_mediaPlayerStates[index].index = index;
    m_mediaPlayerStates[index].playing = (bool)payload.at(7);
    m_mediaPlayerStates[index].loop = (bool)payload.at(8);
    m_mediaPlayerStates[index].atBegining = (bool)payload.at(9);
    m_mediaPlayerStates[index].currentFrame = (quint8)payload.at(11);

    emit mediaPlayerStateChanged(index, m_mediaPlayerStates.value(index));
}

void QAtemConnection::initCommandSlotHash()
{
    m_commandSlotHash.insert("PrgI", "onPrgI");
    m_commandSlotHash.insert("PrvI", "onPrvI");
    m_commandSlotHash.insert("TlIn", "onTlIn");
    m_commandSlotHash.insert("TrPr", "onTrPr");
    m_commandSlotHash.insert("TrPs", "onTrPs");
    m_commandSlotHash.insert("TrSS", "onTrSS");
    m_commandSlotHash.insert("FtbS", "onFtbS");
    m_commandSlotHash.insert("FtbP", "onFtbP");
    m_commandSlotHash.insert("DskS", "onDskS");
    m_commandSlotHash.insert("DskP", "onDskP");
    m_commandSlotHash.insert("DskB", "onDskB");
    m_commandSlotHash.insert("KeOn", "onKeOn");
    m_commandSlotHash.insert("ColV", "onColV");
    m_commandSlotHash.insert("MPCE", "onMPCE");
    m_commandSlotHash.insert("AuxS", "onAuxS");
    m_commandSlotHash.insert("_pin", "on_pin");
    m_commandSlotHash.insert("_ver", "on_ver");
    m_commandSlotHash.insert("InPr", "onInPr");
    m_commandSlotHash.insert("MPSE", "onMPSE");
    m_commandSlotHash.insert("MPfe", "onMPfe");
    m_commandSlotHash.insert("MPCS", "onMPCS");
    m_commandSlotHash.insert("MvIn", "onMvIn");
    m_commandSlotHash.insert("MvPr", "onMvPr");
    m_commandSlotHash.insert("VidM", "onVidM");
    m_commandSlotHash.insert("Time", "onTime");
    m_commandSlotHash.insert("TMxP", "onTMxP");
    m_commandSlotHash.insert("TDpP", "onTDpP");
    m_commandSlotHash.insert("TWpP", "onTWpP");
    m_commandSlotHash.insert("TDvP", "onTDvP");
    m_commandSlotHash.insert("TStP", "onTStP");
    m_commandSlotHash.insert("KeBP", "onKeBP");
    m_commandSlotHash.insert("KeLm", "onKeLm");
    m_commandSlotHash.insert("KeCk", "onKeCk");
    m_commandSlotHash.insert("KePt", "onKePt");
    m_commandSlotHash.insert("KeDV", "onKeDV");
    m_commandSlotHash.insert("KeFS", "onKeFS");
    m_commandSlotHash.insert("DcOt", "onDcOt");
    m_commandSlotHash.insert("AMmO", "onAMmO");
    m_commandSlotHash.insert("MPSp", "onMPSp");
    m_commandSlotHash.insert("RCPS", "onRCPS");
    m_commandSlotHash.insert("AMLv", "onAMLv");
    m_commandSlotHash.insert("AMTl", "onAMTl");
    m_commandSlotHash.insert("AMIP", "onAMIP");
    m_commandSlotHash.insert("AMMO", "onAMMO");
    m_commandSlotHash.insert("LKST", "onLKST");
    m_commandSlotHash.insert("FTCD", "onFTCD");
    m_commandSlotHash.insert("FTDC", "onFTDC");
}

void QAtemConnection::setAudioLevelsEnabled(bool enabled)
{
    QByteArray cmd("SALN");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioInputState(quint16 index, quint8 state)
{
    QByteArray cmd("CAMI");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;

    payload[0] = (char)0x01;
    val.u16 = index;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];
    payload[4] = (char)state;

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioInputBalance(quint16 index, float balance)
{
    QByteArray cmd("CAMI");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;

    payload[0] = (char)0x04;
    val.u16 = index;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];
    val.u16 = balance * 10000;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioInputGain(quint16 index, float gain)
{
    QByteArray cmd("CAMI");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;

    payload[0] = (char)0x06;
    val.u16 = index;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];
    val.u16 = pow(10, gain / 20.0) * 32768;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioMasterOutputGain(float gain)
{
    QByteArray cmd("CAMM");
    QByteArray payload(8, (char)0x0);
    U16_U8 val;

    payload[0] = (char)0x01;
    val.u16 = pow(10, gain / 20.0) * 32768;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::onAMLv(const QByteArray& payload)
{
    // Audio mixer levels
    quint8 numInputs = payload.at(7);

    U16_U8 val;
    val.u8[1] = (quint8)payload.at(11);
    val.u8[0] = (quint8)payload.at(12);
    m_audioMasterOutputLevelLeft = log10f((quint16)val.u16 / 32768.0) * 20.0;
    val.u8[1] = (quint8)payload.at(15);
    val.u8[0] = (quint8)payload.at(16);
    m_audioMasterOutputLevelRight = log10f((quint16)val.u16 / 32768.0) * 20.0;

    QList<quint16> idlist;

    for(int i = 0; i < numInputs; ++i)
    {
        val.u8[1] = (quint8)payload.at(42 + (i * 2));
        val.u8[0] = (quint8)payload.at(43 + (i * 2));
        idlist.append(val.u16);
    }

    int offset = 43 + ((numInputs - 1) * 2) + 4;

    for(int i = 0; i < numInputs; ++i)
    {
        quint16 index = idlist[i];
        m_audioLevels[index].index = index;
        val.u8[1] = (quint8)payload.at(offset + (i * 16));
        val.u8[0] = (quint8)payload.at(offset + 1 + (i * 16));
        m_audioLevels[index].left = log10f((quint16)val.u16 / 32768.0) * 20.0;
        val.u8[1] = (quint8)payload.at(offset + 4 + (i * 16));
        val.u8[0] = (quint8)payload.at(offset + 5 + (i * 16));
        m_audioLevels[index].right = log10f((quint16)val.u16 / 32768.0) * 20.0;
    }

    emit audioLevelsChanged();
}

void QAtemConnection::onAMTl(const QByteArray& payload)
{
    // Audio mixer tally
    quint8 count = (quint8)payload.at(7);
    U16_U8 val;

    for(int i = 0; i < count; ++i)
    {
        val.u8[1] = (quint8)payload.at(8 + (i * 3));
        val.u8[0] = (quint8)payload.at(9 + (i * 3));
        m_audioTally[val.u16] = (bool)payload.at(10 + (i * 3));
    }
}

void QAtemConnection::onAMIP(const QByteArray& payload)
{
    // Audio mixer interface preferences
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);
    quint16 index = val.u16;
    m_audioInputs[index].index = index;
    m_audioInputs[index].type = (quint8)payload.at(8);
    m_audioInputs[index].state = (quint8)payload.at(14);
    val.u8[1] = (quint8)payload.at(16);
    val.u8[0] = (quint8)payload.at(17);
    m_audioInputs[index].gain = log10f((quint16)val.u16 / 32768.0) * 20.0;
    val.u8[1] = (quint8)payload.at(18);
    val.u8[0] = (quint8)payload.at(19);
    m_audioInputs[index].balance = (qint16)val.u16 / 10000.0;

    emit audioInputChanged(index, m_audioInputs[index]);
}

void QAtemConnection::onAMmO(const QByteArray& payload)
{
    m_audioMonitorEnabled = (bool)payload.at(6);
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_audioMonitorGain = log10f((quint16)val.u16 / 32768.0) * 20.0;
    m_audioMonitorMuted = (bool)payload.at(12);
    bool solo = (bool)payload.at(13);

    if(solo)
    {
        m_audioMonitorSolo = (qint8)payload.at(14);
    }
    else
    {
        m_audioMonitorSolo = -1;
    }

    m_audioMonitorDimmed = (bool)payload.at(15);

    emit audioMonitorEnabledChanged(m_audioMonitorEnabled);
    emit audioMonitorGainChanged(m_audioMonitorGain);
    emit audioMonitorMutedChanged(m_audioMonitorMuted);
    emit audioMonitorDimmedChanged(m_audioMonitorDimmed);
    emit audioMonitorSoloChanged(m_audioMonitorSolo);
}

void QAtemConnection::setAudioMonitorEnabled(bool enabled)
{
    if (enabled == m_audioMonitorEnabled)
    {
        return;
    }

    QByteArray cmd("CAMm");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)enabled;

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioMonitorGain(float gain)
{
    QByteArray cmd("CAMm");
    QByteArray payload(12, (char)0x0);
    U16_U8 val;

    payload[0] = (char)0x06;
    val.u16 = pow(10, gain / 20.0) * 32768;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioMonitorMuted(bool muted)
{
    QByteArray cmd("CAMm");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x08;
    payload[6] = (char)muted;

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioMonitorDimmed(bool dimmed)
{
    QByteArray cmd("CAMm");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x40;
    payload[9] = (char)dimmed;

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioMonitorSolo(qint8 solo)
{
    QByteArray cmd("CAMm");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x30;

    if(solo > -1)
    {
        payload[7] = (char)0x01; // Enable
        payload[8] = (char)solo;
    }

    sendCommand(cmd, payload);
}

void QAtemConnection::onAMMO(const QByteArray& payload)
{
    U16_U8 val;
    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);
    m_audioMasterOutputGain = log10f((quint16)val.u16 / 32768.0) * 20.0;

    emit audioMasterOutputGainChanged(m_audioMasterOutputGain);
}

bool QAtemConnection::aquireMediaLock(quint8 id, quint8 index)
{
    if(m_mediaLocks.value(id))
    {
        return false;
    }

    QByteArray cmd("PLCK");
    QByteArray payload(8, (char)0x0);

    payload[1] = (char)id;
    payload[3] = (char)index;
    payload[5] = (char)0x01;

    sendCommand(cmd, payload);
    return true;
}

void QAtemConnection::onLKST(const QByteArray& payload)
{
    quint8 id = payload.at(7);
    m_mediaLocks[id] = payload.at(8);

    emit mediaLockStateChanged(id, m_mediaLocks.value(id));
}

void QAtemConnection::unlockMediaLock(quint8 id)
{
    QByteArray cmd("LOCK");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)id;

    sendCommand(cmd, payload);
}

quint16 QAtemConnection::sendDataToSwitcher(quint8 storeId, quint8 index, const QByteArray &name, const QByteArray &data)
{
    if (m_transferActive)
    {
        return false;
    }

    m_transferStoreId = storeId;
    m_transferIndex = index;
    m_transferName = name;
    m_transferData = data;
    m_lastTransferId++;
    m_transferId = m_lastTransferId;
    m_transferHash = QCryptographicHash::hash(data, QCryptographicHash::Md5);

    initDownloadToSwitcher();

    return true;
}

void QAtemConnection::initDownloadToSwitcher()
{
    QByteArray cmd("FTSD");
    QByteArray payload(16, (char)0x0);

    U16_U8 id;
    id.u16 = m_transferId;
    payload[0] = (char)id.u8[1];
    payload[1] = (char)id.u8[0];
    payload[2] = (char)m_transferStoreId;
    payload[7] = (char)m_transferIndex;
    U32_U8 val;
    val.u32 = m_transferData.size();
    payload[8] = val.u8[3];
    payload[9] = val.u8[2];
    payload[10] = val.u8[1];
    payload[11] = val.u8[0];
    payload[13] = (char)0x01; // 0x01 == write, 0x02 == Clear

    sendCommand(cmd, payload);
}

void QAtemConnection::onFTCD(const QByteArray& payload)
{
    U16_U8 id;
    id.u8[1] = (quint8)payload.at(6);
    id.u8[0] = (quint8)payload.at(7);
    quint8 count = (quint8)payload.at(14);

    if(id.u16 == m_transferId)
    {
        flushTransferBuffer(count);
    }
}

void QAtemConnection::flushTransferBuffer(quint8 count)
{
    int i = 0;

    while(!m_transferData.isEmpty() && i < count)
    {
        QByteArray data = m_transferData.left(1392);
        m_transferData = m_transferData.remove(0, data.size());
        sendData(m_transferId, data);
        m_socket->flush();
        QThread::usleep(50);
        ++i;
    }

    if(!m_transferActive)
    {
        sendFileDescription();
    }

    m_transferActive = !m_transferData.isEmpty();
}

void QAtemConnection::sendData(quint16 id, const QByteArray &data)
{
    QByteArray cmd("FTDa");
    QByteArray payload(data.size() + 8, (char)0x0);

    U16_U8 val;
    val.u16 = id;
    payload[0] = val.u8[1];
    payload[1] = val.u8[0];
    val.u16 = data.size();
    payload[2] = val.u8[1];
    payload[3] = val.u8[0];
    payload.replace(4, val.u16, data);

    sendCommand(cmd, payload);
}

void QAtemConnection::sendFileDescription()
{
    QByteArray cmd("FTFD");
    QByteArray payload(84, (char)0x0);

    U16_U8 val;
    val.u16 = m_transferId;
    payload[0] = val.u8[1];
    payload[1] = val.u8[0];
    payload.replace(2, qMin(64, m_transferName.size()), m_transferName);
    payload.replace(66, 16, m_transferHash);

    sendCommand(cmd, payload);
}

void QAtemConnection::onFTDC(const QByteArray& payload)
{
    U16_U8 id;
    id.u8[1] = (quint8)payload.at(6);
    id.u8[0] = (quint8)payload.at(7);

    emit dataTransferFinished(id.u16);
}

QByteArray QAtemConnection::prepImageForSwitcher(QImage &image, const int width, const int height)
{
    // Size the image
    image.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    if(image.width() > width) {
        image = image.copy((image.width()-width) / 2, 0, width, image.height());
    }
    if(image.height() > height) {
        image = image.copy(0, (image.height()-height) / 2, image.width(), height);
    }
    if(image.width() < width || image.height() < height) {
        QImage blank(width, height, QImage::Format_ARGB32_Premultiplied);
        blank.fill(QColor(0,0,0,0));
        QPainter p(&blank);
        p.drawImage(QPoint((width-image.width()) / 2, (height-image.height()) / 2), image);
        image = blank;
    }

    // Convert pixels in pairs for 4:2:2 compression
    QByteArray data(width*height*4, 0x00);

    const QRgb *pixel = reinterpret_cast<const QRgb*>(image.constBits());
    for(int i = 0; i < width * height; i+=2) {

        unsigned char r1 =   qRed(pixel[i+0]);
        unsigned char g1 = qGreen(pixel[i+0]);
        unsigned char b1 =  qBlue(pixel[i+0]);

        unsigned char r2 =   qRed(pixel[i+1]);
        unsigned char g2 = qGreen(pixel[i+1]);
        unsigned char b2 =  qBlue(pixel[i+1]);

        quint16 a1 = qAlpha(pixel[i+0]) * 3.7;
        quint16 a2 = qAlpha(pixel[i+1]) * 3.7;

        quint16 y1 = (((66  * r1 + 129 * g1 +  25 * b1 + 128) >> 8) + 16 ) * 4 - 1;
        quint16 u1 = (((-38 * r1 -  74 * g1 + 112 * b1 + 128) >> 8) + 128) * 4 - 1;
        quint16 y2 = (((66  * r2 + 129 * g2 +  25 * b2 + 128) >> 8) + 16 ) * 4 - 1;
        quint16 v2 = (((112 * r2 -  94 * g2 -  18 * b2 + 128) >> 8) + 128) * 4 - 1;

        int j = i * 4;

        data[j+0] = a1 >> 4;
        data[j+1] = ((a1 & 0x0f) << 4) | (u1 >> 6);
        data[j+2] = ((u1 & 0x3f) << 2) | (y1 >> 8);
        data[j+3] = y1 & 0xff;

        data[j+4] = a2 >> 4;
        data[j+5] = ((a2 & 0x0f) << 4) | (v2 >> 6);
        data[j+6] = ((v2 & 0x3f) << 2) | (y2 >> 8);
        data[j+7] = y2 & 0xff;
    }

    return data;
}
