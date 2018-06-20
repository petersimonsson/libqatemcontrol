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
#include "qatemcameracontrol.h"
#include "qatemdownstreamkey.h"

#include <QDebug>
#include <QTimer>
#include <QHostAddress>
#include <QImage>
#include <QPainter>
#include <QCryptographicHash>
#include <QThread>

#include <math.h>

#define SIZE_OF_HEADER 0x0c

/// Hack to use QThread::usleep in Qt 4.x
class QAtemThread : public QThread
{
public:
    static void usleep (unsigned long interval) { QThread::usleep(interval); }
};

QAtemConnection::QAtemConnection(QObject* parent)
    : QObject(parent), m_socket(NULL), m_downstreamKeys(2)
{
    m_connectionTimer = new QTimer(this);
    m_connectionTimer->setInterval(1000);
    connect(m_connectionTimer, SIGNAL(timeout()),
            this, SLOT(handleConnectionTimeout()));

    m_port = 9910;
    m_packetCounter = 0;
    m_isInitialized = false;
    m_currentUid = 0;
    m_lastPacketId = 0;

    m_debugEnabled = false;

    m_tallyChannelCount = 0;

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
    m_audioMonitorLevel = 0.0;

    m_audioMasterOutputLevelLeft = 0;
    m_audioMasterOutputLevelRight = 0;
    m_audioMasterOutputPeakLeft = 0.0;
    m_audioMasterOutputPeakRight = 0.0;
    m_audioMasterOutputGain = 0;

    m_audioChannelCount = 0;
    m_hasAudioMonitor = false;

    m_transferActive = false;
    m_transferStoreId = 0;
    m_transferIndex = 0;
    m_transferId = 0;
    m_lastTransferId = 0;

    initCommandSlotHash();

    m_cameraControl = new QAtemCameraControl(this);

    m_downstreamKeys[0] = new QAtemDownstreamKey(0, this);
    m_downstreamKeys[1] = new QAtemDownstreamKey(1, this);

    m_macroInfos.resize(100);
}

QAtemConnection::~QAtemConnection()
{
    qDeleteAll(m_multiViews);
    qDeleteAll(m_mixEffects);
    qDeleteAll(m_downstreamKeys);

    delete m_cameraControl;
    m_cameraControl = NULL;
}

bool QAtemConnection::isConnected() const
{
    return m_socket && m_socket->isValid() && m_isInitialized;
}

void QAtemConnection::connectToSwitcher(const QHostAddress &address, int connectionTimeout)
{
    m_address = address;

    if(m_address.isNull())
    {
        return;
    }

    if (m_socket)
    {
        m_connectionTimer->stop();
        delete m_socket;
        m_socket = NULL;
    }

    m_socket = new QUdpSocket(this);
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(m_socket, SIGNAL(readyRead()),
            this, SLOT(handleSocketData()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handleError(QAbstractSocket::SocketError)));

    m_socket->bind();
    m_packetCounter = 0;
    m_isInitialized = false;
    m_currentUid = 0x1337; // Just a random UID, we'll get a new one from the server eventually
    m_lastPacketId = 0;

    //Hello
    QByteArray datagram = createCommandHeader(Cmd_HelloPacket, 8, m_currentUid, 0x0);
    datagram.append(QByteArray::fromHex("0100000000000000")); // The Hello package needs this... no idea what it means

    sendDatagram(datagram);
    m_connectionTimer->setInterval(connectionTimeout);
    m_connectionTimer->start();
}

void QAtemConnection::disconnectFromSwitcher()
{
    delete m_socket;
    m_socket = NULL;
    m_connectionTimer->stop();
}

void QAtemConnection::handleSocketData()
{
    if(!m_socket)
    {
        return;
    }

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
        else if((m_isInitialized && (header.bitmask & Cmd_AckRequest)) || (!m_isInitialized && datagram.size() == SIZE_OF_HEADER && header.bitmask & Cmd_AckRequest))
        {
            QByteArray ackDatagram = createCommandHeader(Cmd_Ack, 0, header.uid, header.packageId);
            sendDatagram(ackDatagram);
            m_socket->flush();

            if(!m_isInitialized)
            {
                setInitialized(true);
            }
        }

        if((header.packageId - m_lastPacketId) > 1)
        {
            for(int i = 1; i <= (header.packageId - m_lastPacketId - 1); ++i)
            {
                QByteArray resendDatagram = createCommandHeader(Cmd_Resend, 0, m_currentUid, 0, m_lastPacketId + i);
                sendDatagram(resendDatagram);
            }
        }

        m_lastPacketId = header.packageId;

        if(datagram.size() > (SIZE_OF_HEADER + 2) && !(header.bitmask & (Cmd_HelloPacket | Cmd_Resend)))
        {
            parsePayLoad(datagram);
        }

        m_socket->flush();
    }
}

QByteArray QAtemConnection::createCommandHeader(Commands bitmask, quint16 payloadSize, quint16 uid, quint16 ackId, quint16 resendId)
{
    QByteArray buffer(12, (char)0x0);
    quint16 packageId = 0;

    if(!(bitmask & (Cmd_HelloPacket | Cmd_Ack)))
    {
        m_packetCounter++;
        packageId = m_packetCounter;
    }

    QAtem::U16_U8 val;

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

    if(resendId != 0)
    {
        val.u16 = resendId;
        buffer[6] = (char)val.u8[1];
        buffer[7] = (char)val.u8[0];
        buffer[8] = 0x01;
    }

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
    quint16 size = (quint8)datagram.at(offset + 1) | ((quint8)datagram.at(offset) << 8);

    while((offset + size) <= datagram.size())
    {
        QByteArray payload = datagram.mid(offset + 2, size - 2);
//        qDebug() << payload.toHex();

        QByteArray cmd = payload.mid(2, 4); // Skip first two bytes, not sure what they do

        if(cmd == "InCm")
        {
            setInitialized(true);
        }
        else if(cmd == "_MeC")
        {
            quint8 me = (quint8)payload.at(6);
            quint8 keyCount = (quint8)payload.at(7);
            m_mixEffects[me]->createUpstreamKeyers(keyCount);
        }
        else if(cmd == "_MvC")
        {
            quint8 count = (quint8)payload.at(6);
            qDeleteAll(m_multiViews);
            m_multiViews.resize(count);

            for(int i = 0; i < count; ++i)
            {
                m_multiViews[i] = new QAtem::MultiView(i);
            }
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
            size = (quint8)datagram.at(offset + 1) | ((quint8)datagram.at(offset) << 8);
        }
    }
}

void QAtemConnection::setInitialized(bool state)
{
    m_isInitialized = state;

    if(m_isInitialized)
    {
        QMetaObject::invokeMethod(this, "emitConnectedSignal", Qt::QueuedConnection);
    }
}

void QAtemConnection::emitConnectedSignal()
{
    emit connected();
}

bool QAtemConnection::sendDatagram(const QByteArray& datagram)
{
    if(!m_socket)
    {
        return false;
    }

    qint64 sent = m_socket->writeDatagram(datagram, m_address, m_port);

    return sent != -1;
}

bool QAtemConnection::sendCommand(const QByteArray& cmd, const QByteArray& payload)
{
    QAtem::U16_U8 size;

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
    m_connectionTimer->stop();

    emit socketError(m_socket->errorString());

    delete m_socket;
    m_socket = NULL;
    m_isInitialized = false;

    emit disconnected();
}

void QAtemConnection::handleConnectionTimeout()
{
    delete m_socket;
    m_socket = NULL;
    m_isInitialized = false;
    m_connectionTimer->stop();
    emit socketError(tr("The switcher connection timed out"));
    emit disconnected();
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

    QAtem::U16_U8 h, s, l;
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

quint8 QAtemConnection::tallyByIndex(quint8 index) const
{
    if(index < m_tallyByIndex.count())
    {
        return m_tallyByIndex.value(index);
    }

    return 0;
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

    quint8 payloadsize = 4;

    if(m_majorversion == 1 || (m_majorversion == 2 && m_minorversion < 16))
    {
        payloadsize = 8;
    }

    QByteArray cmd("CAuS");
    QByteArray payload(payloadsize, (char)0x0);
    QAtem::U16_U8 val;

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
    QAtem::U16_U8 val;

    payload[0] = (char)0x04;
    val.u16 = input;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];
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
    QAtem::U16_U8 val;

    payload[0] = (char)0x01;
    val.u16 = input;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];
    payload.replace(4, 20, namearray);
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
    QAtem::U16_U8 val;

    payload[0] = (char)0x02;
    val.u16 = input;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];
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

void QAtemConnection::setMediaPoolClipSplit(quint16 size)
{
    QByteArray cmd("CMPS");
    QByteArray payload(4, (char)0x0);

    QAtem::U16_U8 val;
    val.u16 = size;
    payload[0] = (char)val.u8[1];
    payload[1] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setMultiViewLayout(quint8 multiView, quint8 layout)
{
    QByteArray cmd("CMvP");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)multiView;
    payload[2] = (char)layout;

    sendCommand(cmd, payload);
}

void QAtemConnection::setMultiViewInput(quint8 multiView, quint8 windowIndex, quint16 source)
{
    QByteArray cmd("CMvI");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)multiView;
    payload[1] = (char)windowIndex;
    QAtem::U16_U8 val;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::onTlIn(const QByteArray& payload)
{
    QAtem::U16_U8 count;
    count.u8[1] = (quint8)payload.at(6);
    count.u8[0] = (quint8)payload.at(7);
    m_tallyByIndex.resize(count.u16);

    for(quint8 i = 0; i < count.u16; ++i)
    {
        m_tallyByIndex[i] = (quint8)payload.at(8 + i);
    }
}

void QAtemConnection::onColV(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);

    QAtem::U16_U8 h, s, l;

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
    QAtem::U16_U8 val;
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
    QAtem::U16_U8 ver;
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
    QAtem::InputInfo info;
    QAtem::U16_U8 index;
    index.u8[1] = (quint8)payload.at(6);
    index.u8[0] = (quint8)payload.at(7);
    info.index = index.u16;
    info.longText = payload.mid(8, 20);
    info.shortText = payload.mid(28, 4);
    info.availableExternalTypes = (quint8)payload.at(35); // Bit 0: SDI, 1: HDMI, 2: Component, 3: Composite, 4: SVideo
    info.externalType = (quint8)payload.at(37); // 1 = SDI, 2 = HDMI, 3 = Composite, 4 = Component, 5 = SVideo, 0 = Internal
    info.internalType = (quint8)payload.at(38); // 0 = External, 1 = Black, 2 = Color Bars, 3 = Color Generator, 4 = Media Player Fill, 5 = Media Player Key, 6 = SuperSource, 128 = ME Output, 129 = Auxiliary, 130 = Mask
    info.availability = (quint8)payload.at(40); // Bit 0: Auxiliary, 1: Multiviewer, 2: SuperSource Art, 3: SuperSource Box, 4: Key Sources
    info.meAvailability = (quint8)payload.at(41); // Bit 0: ME1 + Fill Sources, 1: ME2 + Fill Sources
    m_inputInfos.insert(info.index, info);

    emit inputInfoChanged(info);
}

void QAtemConnection::onMPSE(const QByteArray& payload)
{
    QAtem::MediaInfo info;
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
    QAtem::MediaInfo info;
    info.type = QAtem::StillMedia;
    info.frameCount = 1;
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
    QAtem::MediaInfo info;
    info.type = QAtem::ClipMedia;
    info.index = (quint8)payload.at(6);
    info.used = (quint8)payload.at(7);

    if(info.used)
    {
        info.name = payload.mid(8, 63);
    }

    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(72);
    val.u8[0] = (quint8)payload.at(73);
    info.frameCount = val.u16;

    m_clipMediaInfos.insert(info.index, info);

    emit mediaInfoChanged(info);
}

void QAtemConnection::onMvIn(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);
    quint8 mvindex = (quint8)payload.at(7);    // Index of multiview output
    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(8); // Index of input, these are mapped in the InPr command
    val.u8[0] = (quint8)payload.at(9);

    if(index < m_multiViews.count() && mvindex < 10)
    {
        m_multiViews[index]->sources[mvindex] = val.u16;
        emit multiViewInputsChanged(index);
    }
}

void QAtemConnection::onMvPr(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);

    if(index < m_multiViews.count())
    {
        m_multiViews[index]->layout = (quint8)payload.at(7);

        emit multiViewLayoutChanged(index, m_multiViews[index]->layout);
    }
}

void QAtemConnection::onVidM(const QByteArray& payload)
{
    m_videoFormat = (quint8)payload.at(6); // 0 = 525i5994, 1 = 625i50, 2 = 525i5994 16:9, 3 = 625i50 16:9, 4 = 720p50, 5 = 720p5994, 6 = 1080i50, 7 = 1080i5994

    emit videoFormatChanged(m_videoFormat);
}

void QAtemConnection::onTime(const QByteArray& payload)
{
    QAtem::U32_U8 val;
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
    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);
    m_mediaPoolClip1Size = val.u16;
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_mediaPoolClip2Size = val.u16;

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
    m_commandSlotHash.insert("_VMC", ObjectSlot(this, "onVMC"));
    m_commandSlotHash.insert("Warn", ObjectSlot(this, "onWarn"));
    m_commandSlotHash.insert("_mpl", ObjectSlot(this, "on_mpl"));
    m_commandSlotHash.insert("_TlC", ObjectSlot(this, "on_TlC"));
    m_commandSlotHash.insert("TlSr", ObjectSlot(this, "onTlSr"));
    m_commandSlotHash.insert("_AMC", ObjectSlot(this, "on_AMC"));
    m_commandSlotHash.insert("MPAS", ObjectSlot(this, "onMPAS"));
    m_commandSlotHash.insert("MPfM", ObjectSlot(this, "onMPfM"));
    m_commandSlotHash.insert("AuxP", ObjectSlot(this, "onAuxP"));
    m_commandSlotHash.insert("MPrp", ObjectSlot(this, "onMPrp"));
    m_commandSlotHash.insert("MRPr", ObjectSlot(this, "onMRPr"));
    m_commandSlotHash.insert("MRcS", ObjectSlot(this, "onMRcS"));
    m_commandSlotHash.insert("_MAC", ObjectSlot(this, "on_MAC"));
    m_commandSlotHash.insert("FTDa", ObjectSlot(this, "onFTDa"));
    m_commandSlotHash.insert("FTDE", ObjectSlot(this, "onFTDE"));
    m_commandSlotHash.insert("LKOB", ObjectSlot(this, "onLKOB"));
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
    QAtem::U16_U8 val;

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
    QAtem::U16_U8 val;

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
    QAtem::U16_U8 val;

    payload[0] = (char)0x06;
    val.u16 = index;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];
    val.u16 = convertFromDecibel(gain);
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setAudioMasterOutputGain(float gain)
{
    QByteArray cmd("CAMM");
    QByteArray payload(8, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)0x01;
    val.u16 = convertFromDecibel(gain);
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::onAMLv(const QByteArray& payload)
{
    // Audio mixer levels
    QAtem::U16_U8 numInputs;
    numInputs.u8[1] = (quint8)payload.at(6);
    numInputs.u8[0] = (quint8)payload.at(7);

    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(11);
    val.u8[0] = (quint8)payload.at(12);
    m_audioMasterOutputLevelLeft = convertToDecibel(val.u16);
    val.u8[1] = (quint8)payload.at(15);
    val.u8[0] = (quint8)payload.at(16);
    m_audioMasterOutputLevelRight = convertToDecibel(val.u16);
    val.u8[1] = (quint8)payload.at(19);
    val.u8[0] = (quint8)payload.at(20);
    m_audioMasterOutputPeakLeft = convertToDecibel(val.u16);
    val.u8[1] = (quint8)payload.at(23);
    val.u8[0] = (quint8)payload.at(24);
    m_audioMasterOutputPeakRight = convertToDecibel(val.u16);
    val.u8[1] = (quint8)payload.at(27);
    val.u8[0] = (quint8)payload.at(28);
    m_audioMonitorLevel = convertToDecibel(val.u16);

    QList<quint16> idlist;

    for(int i = 0; i < numInputs.u16; ++i)
    {
        val.u8[1] = (quint8)payload.at(42 + (i * 2));
        val.u8[0] = (quint8)payload.at(43 + (i * 2));
        idlist.append(val.u16);
    }

    int offset = 43 + ((numInputs.u16) * 2);

    for(int i = 0; i < numInputs.u16; ++i)
    {
        quint16 index = idlist[i];
        m_audioLevels[index].index = index;
        val.u8[1] = (quint8)payload.at(offset + (i * 16));
        val.u8[0] = (quint8)payload.at(offset + 1 + (i * 16));
        m_audioLevels[index].left = convertToDecibel(val.u16);
        val.u8[1] = (quint8)payload.at(offset + 4 + (i * 16));
        val.u8[0] = (quint8)payload.at(offset + 5 + (i * 16));
        m_audioLevels[index].right = convertToDecibel(val.u16);
        val.u8[1] = (quint8)payload.at(offset + 8 + (i * 16));
        val.u8[0] = (quint8)payload.at(offset + 9 + (i * 16));
        m_audioLevels[index].peakLeft = convertToDecibel(val.u16);
        val.u8[1] = (quint8)payload.at(offset + 12 + (i * 16));
        val.u8[0] = (quint8)payload.at(offset + 13 + (i * 16));
        m_audioLevels[index].peakRight = convertToDecibel(val.u16);
    }

    emit audioLevelsChanged();
}

float QAtemConnection::convertToDecibel(quint16 level)
{
    return log10((float)level / 32768.0) * 20.0;
}


quint16 QAtemConnection::convertFromDecibel(float level)
{
    return pow(10,level / 20) * 32768;
}

void QAtemConnection::onAMTl(const QByteArray& payload)
{
    // Audio mixer tally
    QAtem::U16_U8 count;
    count.u8[1] = (quint8)payload.at(6);
    count.u8[0] = (quint8)payload.at(7);
    QAtem::U16_U8 val;

    for(int i = 0; i < count.u16; ++i)
    {
        val.u8[1] = (quint8)payload.at(8 + (i * 3));
        val.u8[0] = (quint8)payload.at(9 + (i * 3));
        m_audioTally[val.u16] = (bool)payload.at(10 + (i * 3));
    }
}

void QAtemConnection::onAMIP(const QByteArray& payload)
{
    // Audio mixer interface preferences
    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);
    quint16 index = val.u16;
    m_audioInputs[index].index = index;
    m_audioInputs[index].type = (quint8)payload.at(8);
    m_audioInputs[index].plugType = (quint8)payload.at(13);
    m_audioInputs[index].state = (quint8)payload.at(14);
    val.u8[1] = (quint8)payload.at(16);
    val.u8[0] = (quint8)payload.at(17);
    m_audioInputs[index].gain = convertToDecibel(val.u16);
    val.u8[1] = (quint8)payload.at(18);
    val.u8[0] = (quint8)payload.at(19);
    m_audioInputs[index].balance = (qint16)val.u16 / 10000.0;

    emit audioInputChanged(index, m_audioInputs[index]);
}

void QAtemConnection::onAMmO(const QByteArray& payload)
{
    m_audioMonitorEnabled = (bool)payload.at(6);
    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_audioMonitorGain = convertToDecibel(val.u16);
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
    QAtem::U16_U8 val;

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
    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);
    m_audioMasterOutputGain = convertToDecibel(val.u16);

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
        return 0;
    }

    m_transferStoreId = storeId;
    m_transferIndex = index;
    m_transferName = name;
    m_transferData = data;
    m_lastTransferId++;
    m_transferId = m_lastTransferId;
    m_transferHash = QCryptographicHash::hash(data, QCryptographicHash::Md5);

    initDownloadToSwitcher();

    return m_transferId;
}

void QAtemConnection::initDownloadToSwitcher()
{
    QByteArray cmd("FTSD");
    QByteArray payload(16, (char)0x0);

    QAtem::U16_U8 id;
    id.u16 = m_transferId;
    payload[0] = (char)id.u8[1];
    payload[1] = (char)id.u8[0];
    payload[2] = (char)m_transferStoreId;
    payload[7] = (char)m_transferIndex;
    QAtem::U32_U8 val;
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
    QAtem::U16_U8 id;
    id.u8[1] = (quint8)payload.at(6);
    id.u8[0] = (quint8)payload.at(7);
    quint8 count = (quint8)payload.at(15);

    if(id.u16 == m_transferId)
    {
        flushTransferBuffer(count);
    }
}

void QAtemConnection::flushTransferBuffer(quint8 count)
{
    int i = 0;

    while(!m_transferData.isEmpty() && i < count && m_socket)
    {
        QByteArray data = m_transferData.left(1392);
        m_transferData = m_transferData.remove(0, data.size());
        sendData(m_transferId, data);
        m_socket->flush();
        QAtemThread::usleep(50); // QAtemThread is a hack to support Qt 4.x
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
    QByteArray payload(data.size() + 4, (char)0x0);

    QAtem::U16_U8 val;
    val.u16 = id;
    payload[0] = val.u8[1];
    payload[1] = val.u8[0];
    val.u16 = data.size();
    payload[2] = val.u8[1];
    payload[3] = val.u8[0];
    payload.replace(4, data.size(), data);

    sendCommand(cmd, payload);
}

void QAtemConnection::sendFileDescription()
{
    QByteArray cmd("FTFD");
    QByteArray payload(212, (char)0x0);

    QAtem::U16_U8 val;
    val.u16 = m_transferId;
    payload[0] = val.u8[1];
    payload[1] = val.u8[0];
    payload.replace(2, qMin(194, m_transferName.size()), m_transferName);
    payload.replace(194, 16, m_transferHash);

    sendCommand(cmd, payload);
}

void QAtemConnection::onFTDC(const QByteArray& payload)
{
    QAtem::U16_U8 id;
    id.u8[1] = (quint8)payload.at(6);
    id.u8[0] = (quint8)payload.at(7);

    emit dataTransferFinished(id.u16);
}

quint16 QAtemConnection::getDataFromSwitcher(quint8 storeId, quint8 index)
{
    if (m_transferActive)
    {
        return 0;
    }

    m_transferStoreId = storeId;
    m_transferIndex = index;
    m_lastTransferId++;
    m_transferId = m_lastTransferId;
    m_transferActive = true;
    m_transferData.clear();

    requestData();

    return m_transferId;
}

void QAtemConnection::requestData()
{
    QByteArray cmd("FTSU");
    QByteArray payload(12, (char)0x0);

    QAtem::U16_U8 id;
    id.u16 = m_transferId;
    payload[0] = (char)id.u8[1];
    payload[1] = (char)id.u8[0];
    payload[2] = (char)m_transferStoreId;
    payload[7] = (char)m_transferIndex;

    if(m_transferStoreId == 0xff) // Macros
    {
        payload[8] = (char)0x03;
    }

    sendCommand(cmd, payload);
}

void QAtemConnection::aquireLock(quint8 storeId)
{
    QByteArray cmd("LOCK");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)storeId;
    payload[2] = (char)0x01;

    sendCommand(cmd, payload);
}

void QAtemConnection::onLKOB(const QByteArray& payload)
{
    emit getLockStateChanged(payload.at(7), true);
}

void QAtemConnection::onFTDa(const QByteArray& payload)
{
    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);

    if(val.u16 != m_transferId)
    {
        qWarning() << "Unknown transfer ID:" << val.u16 << "(" << m_transferId << ")";
        return;
    }

    val.u8[1] = (quint8)payload.at(8);
    val.u8[0] = (quint8)payload.at(9);
    m_transferData.append(payload.mid(10, val.u16));

    QTimer::singleShot(50, this, SLOT(acceptData()));
}

void QAtemConnection::acceptData()
{
    QByteArray cmd("FTUA");
    QByteArray payload(4, (char)0x0);

    QAtem::U16_U8 val;
    val.u16 = m_transferId;
    payload[0] = (char)val.u8[1];
    payload[1] = (char)val.u8[0];
    payload[3] = (char)m_transferIndex;

    sendCommand(cmd, payload);
}

void QAtemConnection::onFTDE(const QByteArray& payload)
{
    qWarning() << "Data transfer error:" << payload.toHex();
}

QByteArray QAtemConnection::prepImageForSwitcher(QImage &image, const int width, const int height)
{
    // Size the image
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);

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
    m_topology.downstreamKeyers = (quint8)payload.at(11);
    m_topology.upstreamKeyers = (quint8)payload.at(13);
    m_topology.stingers = (quint8)payload.at(14);
    m_topology.DVEs = (quint8)payload.at(15);
    m_topology.supersources = (quint8)payload.at(16);
    m_topology.hasSD = (bool)payload.at(17);

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

QAtemDownstreamKey *QAtemConnection::downstreamKey(quint8 id) const
{
    if (id < m_downstreamKeys.count())
        return m_downstreamKeys.value(id);
    else
        return NULL;
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

void QAtemConnection::onVMC(const QByteArray& payload)
{
    QAtem::U16_U8 val;

    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);

    QVector<QAtem::VideoMode> mode(18);
    mode[0] = QAtem::VideoMode(0, "525i59.94 NTSC", QSize(720, 525), 29.97f);
    mode[1] = QAtem::VideoMode(1, "625i50 PAL", QSize(720, 625), 25);
    mode[2] = QAtem::VideoMode(2, "525i59.94 NTSC 16:9", QSize(864, 525), 29.97f);
    mode[3] = QAtem::VideoMode(3, "625i50 PAL 16:9", QSize(1024, 625), 25);
    mode[4] = QAtem::VideoMode(4, "720p50", QSize(1280, 720), 50);
    mode[5] = QAtem::VideoMode(5, "720p59.94", QSize(1280, 720), 59.94f);
    mode[6] = QAtem::VideoMode(6, "1080i50", QSize(1920, 1080), 25);
    mode[7] = QAtem::VideoMode(7, "1080i59.94", QSize(1920, 1080), 29.97f);
    mode[8] = QAtem::VideoMode(8, "1080p23.98", QSize(1920, 1080), 23.98f);
    mode[9] = QAtem::VideoMode(9, "1080p24", QSize(1920, 1080), 24);
    mode[10] = QAtem::VideoMode(10, "1080p25", QSize(1920, 1080), 25);
    mode[11] = QAtem::VideoMode(11, "1080p29.97", QSize(1920, 1080), 29.97f);
    mode[12] = QAtem::VideoMode(12, "1080p50", QSize(1920, 1080), 50);
    mode[13] = QAtem::VideoMode(13, "1080p59.94", QSize(1920, 1080), 59.94f);
    mode[14] = QAtem::VideoMode(14, "2160p23.98", QSize(3840, 2160), 23.98f);
    mode[15] = QAtem::VideoMode(15, "2160p24", QSize(3840, 2160), 24);
    mode[16] = QAtem::VideoMode(16, "2160p25", QSize(3840, 2160), 25);
    mode[17] = QAtem::VideoMode(17, "2160p29.97", QSize(3840, 2160), 29.97f);

    m_availableVideoModes.clear();

    for(int i = 0; i < val.u16; ++i)
    {
        m_availableVideoModes.insert(i, mode[i]);
    }
}

QAtem::MultiView *QAtemConnection::multiView(quint8 index) const
{
    if(index < m_multiViews.count())
    {
        return m_multiViews[index];
    }
    else
    {
        return NULL;
    }
}

void QAtemConnection::onWarn(const QByteArray& payload)
{
    QString text = payload.mid(6);

    emit switcherWarning(text);
}

void QAtemConnection::on_mpl(const QByteArray& payload)
{
    m_mediaPoolStillBankCount = (quint8)payload.at(6);
    m_mediaPoolClipBankCount = (quint8)payload.at(7);
}

void QAtemConnection::on_TlC(const QByteArray& payload)
{
    QAtem::U16_U8 val;
    val.u8[1] = (quint8)payload.at(6);
    val.u8[0] = (quint8)payload.at(7);
    m_tallyChannelCount = val.u16;
}

void QAtemConnection::onTlSr(const QByteArray& payload)
{
    QAtem::U16_U8 count;
    count.u8[1] = (quint8)payload.at(6);
    count.u8[0] = (quint8)payload.at(7);

    QAtem::U16_U8 index;

    for(int i = 0; i < count.u16; ++i)
    {
        index.u8[1] = (quint8)payload.at(8 + (i * 3));
        index.u8[0] = (quint8)payload.at(9 + (i * 3));

        if(m_inputInfos.contains(index.u16))
        {
            m_inputInfos[index.u16].tally = (quint8)payload.at(10 + (i * 3));
        }
    }

    emit tallyStatesChanged();
}

void QAtemConnection::resetAudioMasterOutputPeaks()
{
    QByteArray cmd("RAMP");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x04;
    payload[4] = (char)0x01;
    sendCommand(cmd, payload);
}

void QAtemConnection::resetAudioInputPeaks(quint16 input)
{
    QByteArray cmd("RAMP");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x02;
    QAtem::U16_U8 val;
    val.u16 = input;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::on_AMC(const QByteArray& payload)
{
    m_audioChannelCount = (quint8)payload.at(6);
    m_hasAudioMonitor = (quint8)payload.at(7);
}

void QAtemConnection::onMPAS(const QByteArray& payload)
{
    QAtem::MediaInfo info;
    info.type = QAtem::SoundMedia;
    info.frameCount = 1;
    info.index = (quint8)payload.at(6);
    info.used = (quint8)payload.at(7);

    if(info.used)
    {
        info.name = payload.mid(24);
    }

    m_soundMediaInfos.insert(info.index, info);
}

void QAtemConnection::onMPfM(const QByteArray& payload)
{
    if(debugEnabled())
    {
        qDebug() << "MPfM:" << payload.mid(6).toHex();
    }
}

void QAtemConnection::onAuxP(const QByteArray& payload)
{
    if(debugEnabled())
    {
        quint8 index = (quint8)payload.at(6);
        qDebug() << "AuxP for aux" << index <<":" << payload.mid(7).toHex();
    }
}

void QAtemConnection::onMPrp(const QByteArray& payload)
{
    QAtem::MacroInfo info;
    QAtem::U16_U8 lenName, lenDesc;

    info.index = (quint8)payload.at(7);
    info.used = (bool)payload.at(8);
    lenName.u8[1] = (quint8)payload.at(10);
    lenName.u8[0] = (quint8)payload.at(11);
    lenDesc.u8[1] = (quint8)payload.at(12);
    lenDesc.u8[0] = (quint8)payload.at(13);

    if(lenName.u16 > 0)
    {
        info.name = payload.mid(14, lenName.u16);
    }
    if(lenDesc.u16 > 0)
    {
        info.description = payload.mid(14 + lenName.u16, lenDesc.u16);
    }

    m_macroInfos[info.index] = info;
    emit macroInfoChanged(info.index, info);
}

void QAtemConnection::onMRPr(const QByteArray& payload)
{
    m_macroRunningState = (QAtem::MacroRunningState)payload.at(6);
    m_macroRepeating = (bool)payload.at(7);
    m_runningMacro = (quint8)payload.at(9);

    emit macroRunningStateChanged(m_macroRunningState, m_macroRepeating, m_runningMacro);
}

void QAtemConnection::onMRcS(const QByteArray& payload)
{
    m_macroRecording = (bool)payload.at(6);
    m_recordingMacro = (quint8)payload.at(9);

    emit macroRecordingStateChanged(m_macroRecording, m_recordingMacro);
}

void QAtemConnection::on_MAC(const QByteArray& payload)
{
    m_macroInfos.resize((quint8)payload.at(6));
}

void QAtemConnection::runMacro(quint8 macroIndex)
{
    QByteArray cmd("MAct");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)macroIndex;

    sendCommand(cmd, payload);
}

void QAtemConnection::setMacroRepeating(bool state)
{
    QByteArray cmd("MRCP");
    QByteArray payload(4, (char)0x0);

    payload[0] = 0x01;
    payload[1] = (char)state;

    sendCommand(cmd, payload);
}

void QAtemConnection::startRecordingMacro(quint8 macroIndex, const QByteArray &name, const QByteArray &description)
{
    QByteArray cmd("MSRc");
    QByteArray payload(6, (char)0x0);

    payload[1] = (char)macroIndex;
    QAtem::U16_U8 val;
    val.u16 = name.count();
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];
    val.u16 = description.count();
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];
    payload += name;
    payload += description;

    sendCommand(cmd, payload);
}

void QAtemConnection::stopRecordingMacro()
{
    QByteArray cmd("MAct");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0xff;
    payload[1] = (char)0xff;
    payload[2] = (char)0x02;

    sendCommand(cmd, payload);
}

void QAtemConnection::addMacroUserWait()
{
    QByteArray cmd("MAct");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0xff;
    payload[1] = (char)0xff;
    payload[2] = (char)0x03;

    sendCommand(cmd, payload);
}

void QAtemConnection::addMacroPause(quint32 frames)
{
    QByteArray cmd("MSlp");
    QByteArray payload(4, (char)0x0);

    QAtem::U32_U8 val;
    val.u32 = frames;
    payload[0] = (char)val.u8[3];
    payload[1] = (char)val.u8[2];
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    sendCommand(cmd, payload);
}

void QAtemConnection::setMacroName(quint8 macroIndex, const QByteArray &name)
{
    QByteArray cmd("CMPr");
    QByteArray payload(6, (char)0x0);

    payload[0] = (char)0x01;
    payload[3] = (char)macroIndex;
    QAtem::U16_U8 val;
    val.u16 = name.count();
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];
    payload += name;

    sendCommand(cmd, payload);
}

void QAtemConnection::setMacroDescription(quint8 macroIndex, const QByteArray &description)
{
    QByteArray cmd("CMPr");
    QByteArray payload(6, (char)0x0);

    payload[0] = (char)0x02;
    payload[3] = (char)macroIndex;
    QAtem::U16_U8 val;
    val.u16 = description.count();
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];
    payload += description;

    sendCommand(cmd, payload);
}

void QAtemConnection::removeMacro(quint8 macroIndex)
{
    QByteArray cmd("MAct");
    QByteArray payload(4, (char)0x0);

    payload[1] = (char)macroIndex;
    payload[2] = (char)0x05;

    sendCommand(cmd, payload);
}

void QAtemConnection::continueMacro()
{
    QByteArray cmd("MAct");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0xff;
    payload[1] = (char)0xff;
    payload[2] = (char)0x04;

    sendCommand(cmd, payload);
}

void QAtemConnection::stopMacro()
{
    QByteArray cmd("MAct");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0xff;
    payload[1] = (char)0xff;
    payload[2] = (char)0x01;

    sendCommand(cmd, payload);
}
