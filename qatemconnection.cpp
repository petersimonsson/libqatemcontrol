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
#include "qatemmixeffect.h"

#include <QDebug>
#include <QTimer>
#include <QHostAddress>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <QCryptographicHash>

#include <math.h>

#define SIZE_OF_HEADER 0x0c

// Wrapping the QThread library if compiling in a QT version less then version 5
// and exposing the protected method usleep.
ouif QT_VERSION <= 0x050000
class Thread : public QThread
{
public:
    static void usleep(unsigned long usecs)
    {
        QThread::usleep(usecs);
    }
};
#endif

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

    m_tallyStateCount = 0;

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

QAtemConnection::~QAtemConnection()
{
    qDeleteAll(m_mixEffects);
}

bool QAtemConnection::isConnected() const
{
    return m_socket && m_socket->isOpen() && m_isInitialized;
}

void QAtemConnection::connectToSwitcher(const QHostAddress &address, int connectionTimeout)
{
    m_address = address;

    if(m_address.isNull())
    {
        return;
    }

    if (m_socket->isOpen())
    {
        m_connectionTimer->stop();
        m_socket->close();
    }

    m_socket->bind();
    m_packetCounter = 0;
    m_isInitialized = false;
    m_currentUid = 0x1337; // Just a random UID, we'll get a new one from the server eventually

    //Hello
    QByteArray datagram = createCommandHeader(Cmd_HelloPacket, 8, m_currentUid, 0x0);
    datagram.append(QByteArray::fromHex("0100000000000000")); // The Hello package needs this... no idea what it means

    sendDatagram(datagram);
    m_connectionTimer->setInterval(connectionTimeout);
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
        else if(cmd == "_MeC")
        {
            quint8 me = (quint8)payload.at(6);
            quint8 keyCount = (quint8)payload.at(7);
            m_mixEffects[me]->createUpstreamKeyers(keyCount);
        }
        else if(m_commandSlotHash.contains(cmd))
        {
            if(cmd == "_top")
            {
                quint8 meCount = (quint8)payload.at(6);
                qDeleteAll(m_mixEffects);
                m_mixEffects.resize(meCount);

                for(int i = 0; i < meCount; ++i)
                {
                    QAtemMixEffect *me = new QAtemMixEffect(i, this);
                    m_mixEffects[i] = me;
                }
            }

            foreach(const ObjectSlot &objslot, m_commandSlotHash.values(cmd))
            {
                QMetaObject::invokeMethod(objslot.object, objslot.slot, Qt::QueuedConnection, Q_ARG(QByteArray, payload));
            }
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
    if(type == m_inputInfos.value(input).externalType)
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

void QAtemConnection::onTlIn(const QByteArray& payload)
{
    m_tallyStateCount = payload.at(7);

    for(quint8 i = 0; i < m_tallyStateCount; ++i)
    {
        m_tallyStates[i] = (quint8)payload.at(8 + i);
    }

    emit tallyStatesChanged();
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
    info.availableExternalTypes = (quint8)payload.at(33); // Bit 0: SDI, 1: HDMI, 2: Component, 3: Composite, 4: SVideo
    info.externalType = (quint8)payload.at(35); // 1 = SDI, 2 = HDMI, 3 = Composite, 4 = Component, 5 = SVideo, 0 = Internal
    info.internalType = (quint8)payload.at(36); // 0 = External, 1 = Black, 2 = Color Bars, 3 = Color Generator, 4 = Media Player Fill, 5 = Media Player Key, 6 = SuperSource, 128 = ME Output, 129 = Auxiliary, 130 = Mask
    info.availability = (quint8)payload.at(38); // Bit 0: Auxiliary, 1: Multiviewer, 2: SuperSource Art, 3: SuperSource Box, 4: Key Sources
    info.meAvailability = (quint8)payload.at(39); // Bit 0: ME1 + Fill Sources, 1: ME2 + Fill Sources
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
    m_commandSlotHash.insert("TlIn", ObjectSlot(this, "onTlIn"));
    m_commandSlotHash.insert("DskS", ObjectSlot(this, "onDskS"));
    m_commandSlotHash.insert("DskP", ObjectSlot(this, "onDskP"));
    m_commandSlotHash.insert("DskB", ObjectSlot(this, "onDskB"));
    m_commandSlotHash.insert("ColV", ObjectSlot(this, "onColV"));
    m_commandSlotHash.insert("MPCE", ObjectSlot(this, "onMPCE"));
    m_commandSlotHash.insert("AuxS", ObjectSlot(this, "onAuxS"));
    m_commandSlotHash.insert("_pin", ObjectSlot(this, "on_pin"));
    m_commandSlotHash.insert("_ver", ObjectSlot(this, "on_ver"));
    m_commandSlotHash.insert("InPr", ObjectSlot(this, "onInPr"));
    m_commandSlotHash.insert("MPSE", ObjectSlot(this, "onMPSE"));
    m_commandSlotHash.insert("MPfe", ObjectSlot(this, "onMPfe"));
    m_commandSlotHash.insert("MPCS", ObjectSlot(this, "onMPCS"));
    m_commandSlotHash.insert("MvIn", ObjectSlot(this, "onMvIn"));
    m_commandSlotHash.insert("MvPr", ObjectSlot(this, "onMvPr"));
    m_commandSlotHash.insert("VidM", ObjectSlot(this, "onVidM"));
    m_commandSlotHash.insert("Time", ObjectSlot(this, "onTime"));
    m_commandSlotHash.insert("DcOt", ObjectSlot(this, "onDcOt"));
    m_commandSlotHash.insert("AMmO", ObjectSlot(this, "onAMmO"));
    m_commandSlotHash.insert("MPSp", ObjectSlot(this, "onMPSp"));
    m_commandSlotHash.insert("RCPS", ObjectSlot(this, "onRCPS"));
    m_commandSlotHash.insert("AMLv", ObjectSlot(this, "onAMLv"));
    m_commandSlotHash.insert("AMTl", ObjectSlot(this, "onAMTl"));
    m_commandSlotHash.insert("AMIP", ObjectSlot(this, "onAMIP"));
    m_commandSlotHash.insert("AMMO", ObjectSlot(this, "onAMMO"));
    m_commandSlotHash.insert("LKST", ObjectSlot(this, "onLKST"));
    m_commandSlotHash.insert("FTCD", ObjectSlot(this, "onFTCD"));
    m_commandSlotHash.insert("FTDC", ObjectSlot(this, "onFTDC"));
    m_commandSlotHash.insert("_top", ObjectSlot(this, "on_top"));
    m_commandSlotHash.insert("Powr", ObjectSlot(this, "onPowr"));
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
        #if QT_VERSION >= 0x050000
          QThread::usleep(50);
        #else
          Thread::usleep(50);
        #endif
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

void QAtemConnection::on_top(const QByteArray& payload)
{
    m_topology.MEs = (quint8)payload.at(6);
    m_topology.sources = (quint8)payload.at(7);
    m_topology.colorGenerators = (quint8)payload.at(8);
    m_topology.auxBusses = (quint8)payload.at(9);
    m_topology.downstreamKeyers = (quint8)payload.at(10);
    m_topology.stingers = (quint8)payload.at(11);
    m_topology.DVEs = (quint8)payload.at(12);
    m_topology.supersources = (quint8)payload.at(13);
    m_topology.hasSD = (bool)payload.at(15);

    emit topologyChanged(m_topology);
}

QAtemMixEffect *QAtemConnection::mixEffect(quint8 me) const
{
    if(me < m_mixEffects.count())
    {
        return m_mixEffects[me];
    }
    else
    {
        return NULL;
    }
}

void QAtemConnection::registerCommand(const QByteArray &command, QObject *object, const QByteArray &slot)
{
    m_commandSlotHash.insert(command, ObjectSlot(object, slot));
}

void QAtemConnection::unregisterCommand(const QByteArray &command, QObject *object)
{
    foreach(const ObjectSlot &objslot, m_commandSlotHash.values(command))
    {
        if(objslot.object == object)
        {
            m_commandSlotHash.remove(command, objslot);
        }
    }
}

void QAtemConnection::onPowr(const QByteArray& payload)
{
    m_powerStatus = (quint8)payload.at(6); // Bit 0: Main power on/off 1: Backup power on/off

    emit powerStatusChanged(m_powerStatus);
}
