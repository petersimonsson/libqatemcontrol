/*
Copyright 2012  Peter Simonsson <peter.simonsson@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of
the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "qatemconnection.h"

#include <QDebug>

#define SIZE_OF_HEADER 0x0c

QAtemConnection::QAtemConnection(QObject* parent)
    : QObject(parent)
{
    m_socket = new QUdpSocket(this);
    m_socket->bind();

    connect(m_socket, SIGNAL(readyRead()),
            this, SLOT(handleSocketData()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handleError(QAbstractSocket::SocketError)));

    m_port = 9910;
    m_packetCounter = 0;
    m_isInitialized = false;
    m_currentUid = 0;

    m_programInput = 0;
    m_previewInput = 0;
    m_tallyStateCount = 0;

    m_transitionPreviewEnabled = false;
    m_transitionFrameCount = 0;
    m_transitionPosition = 0;
    m_keyersOnNextTransition = 0;
    m_transitionStyle = 0;

    m_fadeToBlackEnabled = false;
    m_fadeToBlackFrameCount = 0;

    m_mixFrames = 0;

    m_dipFrames = 0;

    m_wipeFrames = 0;
    m_wipeBorderWidth = 0;
    m_wipeBorderSoftness = 0;
    m_wipeType = 0;
    m_wipeSymmetry = 0;
    m_wipeXPosition = 0;
    m_wipeYPosition = 0;
    m_wipeReverseDirection = false;
    m_wipeFlipFlop = false;

    m_dveFrames = 0;

    m_stingFrames = 0;

    m_borderSource = 0;

    m_multiViewLayoutIndex = 0;

    m_videoFormatIndex = 0;

    m_majorversion = 0;
    m_minorversion = 0;
}

void QAtemConnection::connectToSwitcher(const QHostAddress &address)
{
    m_address = address;

    if(m_address.isNull())
    {
        return;
    }

    m_packetCounter = 0;
    m_isInitialized = false;
    m_currentUid = 0x1337; // Just a random UID, we'll get a new one from the server eventually

    //Hello
    QByteArray datagram = createCommandHeader(Cmd_HelloPacket, 8, m_currentUid, 0x0, 0x0, 0x0);
    datagram.append(QByteArray::fromHex("0100000000000000")); // The Hello package needs this... no idea what it means

    sendDatagram(datagram);
}

void QAtemConnection::handleSocketData()
{
    while (m_socket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());

        m_socket->readDatagram(datagram.data(), datagram.size());

//        qDebug() << datagram.toHex();

        QAtemConnection::CommandHeader header = parseCommandHeader(datagram);
        m_currentUid = header.uid;

        if(!m_isInitialized && !(header.bitmask & Cmd_Ack))
        {
            m_isInitialized = (header.size == SIZE_OF_HEADER);

            if(m_isInitialized)
            {
                emit connected();
            }
        }

        if(header.bitmask & Cmd_HelloPacket)
        {
            QByteArray ackDatagram = createCommandHeader(Cmd_Ack, 0, header.uid, 0x0, 0x0, 0x0);
            sendDatagram(ackDatagram);
        }
        else if(m_isInitialized && (header.bitmask & Cmd_AckRequest))
        {
            QByteArray ackDatagram = createCommandHeader(Cmd_Ack, 0, header.uid, header.packageId, 0x0, 0xcc /*No idea what this means but looks good in WireShark*/);
            sendDatagram(ackDatagram);
        }

        if(datagram.size() > (SIZE_OF_HEADER + 2) && !(header.bitmask & Cmd_HelloPacket))
        {
            parsePayLoad(datagram);
        }
    }
}

QByteArray QAtemConnection::createCommandHeader(Commands bitmask, quint16 payloadSize, quint16 uid, quint16 ackId, quint16 undefined1 , quint16 undefined2)
{
    QByteArray buffer;
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
    buffer.append(val.u8[1]);
    buffer.append(val.u8[0]);

    val.u16 = uid;
    buffer.append(val.u8[1]);
    buffer.append(val.u8[0]);

    val.u16 = ackId;
    buffer.append(val.u8[1]);
    buffer.append(val.u8[0]);

    // Unkown bits
    val.u16 = undefined1;
    buffer.append(val.u8[1]);
    buffer.append(val.u8[0]);
    val.u16 = undefined2;
    buffer.append(val.u8[1]);
    buffer.append(val.u8[0]); // Usually set to 0xCC in ACKs

    val.u16 = packageId;
    buffer.append(val.u8[1]);
    buffer.append(val.u8[0]);

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

        if(cmd == "PrgI")
        {
            m_programInput = (quint8)payload.at(7);
            emit programInputChanged(m_programInput);
        }
        else if(cmd == "PrvI")
        {
            m_previewInput = (quint8)payload.at(7);
            emit previewInputChanged(m_previewInput);
        }
        else if(cmd == "TlIn")
        {
            m_tallyStateCount = payload.at(7);

            for(quint8 i = 0; i < m_tallyStateCount; ++i)
            {
                m_tallyStates[i] = (quint8)payload.at(8 + i);
            }

            emit tallyStatesChanged();
        }
        else if(cmd == "TrPr")
        {
            m_transitionPreviewEnabled = (payload.at(7) > 0);

            emit transitionPreviewChanged(m_transitionPreviewEnabled);
        }
        else if(cmd == "TrPs")
        {
            m_transitionFrameCount = (quint8)payload.at(8);
            m_transitionPosition = ((quint8)payload.at(11) | ((quint8)payload.at(10) << 8));

            emit transitionFrameCountChanged(m_transitionFrameCount);
            emit transitionPositionChanged(m_transitionPosition);
        }
        else if(cmd == "TrSS")
        {
            m_transitionStyle = (quint8)payload.at(7); // Bit 0 = Mix, 1 = Dip, 2 = Wipe, 3 = DVE and 4 = Sting, only bit 0-2 available on TVS
            m_keyersOnNextTransition = ((quint8)payload.at(8) & 0x1f); // Bit 0 = Background, 1-4 = keys, only bit 0 and 1 available on TVS

            emit transitionStyleChanged(m_transitionStyle);
            emit keyersOnNextTransitionChanged(m_keyersOnNextTransition);
        }
        else if(cmd == "FtbS")
        {
            m_fadeToBlackEnabled = (quint8)payload.at(7);
            m_fadeToBlackFrameCount = (quint8)payload.at(8);

            emit fadeToBlackChanged(m_fadeToBlackEnabled);
            emit fadeToBlackFrameCountChanged(m_fadeToBlackFrameCount);
        }
        else if(cmd == "FtbP")
        {
            m_fadeToBlackFrames = (quint8)payload.at(7);

            emit fadeToBlackFramesChanged(m_fadeToBlackFrames);
        }
        else if(cmd == "DskS")
        {
            quint8 index = (quint8)payload.at(6);
            m_downstreamKey[index].m_onAir = (quint8)payload.at(7);
            m_downstreamKey[index].m_frameCount = (quint8)payload.at(10);

            emit downstreamKeyOnChanged(index, m_downstreamKey[index].m_onAir);
            emit downstreamKeyFrameCountChanged(index, m_downstreamKey[index].m_frameCount);
        }
        else if(cmd == "DskP")
        {
            quint8 index = (quint8)payload.at(6);
            m_downstreamKey[index].m_tie = (quint8)payload.at(7);
            m_downstreamKey[index].m_frames = (quint8)payload.at(8);
            m_downstreamKey[index].m_preMultiplied = (quint8)payload.at(9);
            U16_U8 val;
            val.u8[1] = (quint8)payload.at(10);
            val.u8[0] = (quint8)payload.at(11);
            m_downstreamKey[index].m_clip = val.u16 / 10.0;
            val.u8[1] = (quint8)payload.at(12);
            val.u8[0] = (quint8)payload.at(13);
            m_downstreamKey[index].m_gain = val.u16 / 10.0;
            m_downstreamKey[index].m_invertKey = (quint8)payload.at(14);
            m_downstreamKey[index].m_enableMask = (quint8)payload.at(15);
            val.u8[1] = (quint8)payload.at(16);
            val.u8[0] = (quint8)payload.at(17);
            m_downstreamKey[index].m_topMask = (qint16)val.u16 / 1000.0;
            val.u8[1] = (quint8)payload.at(18);
            val.u8[0] = (quint8)payload.at(19);
            m_downstreamKey[index].m_bottomMask = (qint16)val.u16 / 1000.0;
            val.u8[1] = (quint8)payload.at(20);
            val.u8[0] = (quint8)payload.at(21);
            m_downstreamKey[index].m_leftMask = (qint16)val.u16 / 1000.0;
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            m_downstreamKey[index].m_rightMask = (qint16)val.u16 / 1000.0;

            emit downstreamKeyTieChanged(index, m_downstreamKey[index].m_tie);
            emit downstreamKeyFramesChanged(index, m_downstreamKey[index].m_frames);
            emit downstreamKeyInvertKeyChanged(index, m_downstreamKey[index].m_invertKey);
            emit downstreamKeyPreMultipliedChanged(index, m_downstreamKey[index].m_preMultiplied);
            emit downstreamKeyClipChanged(index, m_downstreamKey[index].m_clip);
            emit downstreamKeyGainChanged(index, m_downstreamKey[index].m_gain);
            emit downstreamKeyEnableMaskChanged(index, m_downstreamKey[index].m_enableMask);
            emit downstreamKeyTopMaskChanged(index, m_downstreamKey[index].m_topMask);
            emit downstreamKeyBottomMaskChanged(index, m_downstreamKey[index].m_bottomMask);
            emit downstreamKeyLeftMaskChanged(index, m_downstreamKey[index].m_leftMask);
            emit downstreamKeyRightMaskChanged(index, m_downstreamKey[index].m_rightMask);
        }
        else if(cmd == "DskB")
        {
            quint8 index = (quint8)payload.at(6);
            m_downstreamKey[index].m_fillSource = (quint8)payload.at(7);
            m_downstreamKey[index].m_keySource = (quint8)payload.at(8);

            emit downstreamKeySourcesChanged(index, m_downstreamKey[index].m_fillSource, m_downstreamKey[index].m_keySource);
        }
        else if(cmd == "KeOn")
        {
            quint8 index = (quint8)payload.at(7);
            m_upstreamKeyOn[index] = (quint8)payload.at(8);

            emit upstreamKeyOnChanged(index, m_upstreamKeyOn[index]);
        }
        else if(cmd == "ColV")
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
        else if(cmd == "MPCE")
        {
            quint8 index = (quint8)payload.at(6);

            m_mediaPlayerType[index] = (quint8)payload.at(7);
            m_mediaPlayerSelectedStill[index] = (quint8)payload.at(8);
            m_mediaPlayerSelectedClip[index] = (quint8)payload.at(9);

            emit mediaPlayerChanged(index, m_mediaPlayerType[index], m_mediaPlayerSelectedStill[index], m_mediaPlayerSelectedClip[index]);
        }
        else if(cmd == "AuxS")
        {
            quint8 index = (quint8)payload.at(6);

            m_auxSource[index] = (quint8)payload.at(7);

            emit auxSourceChanged(index, m_auxSource[index]);
        }
        else if(cmd == "_pin")
        {
            m_productInformation = payload.mid(6);

            emit productInformationChanged(m_productInformation);
        }
        else if(cmd == "_ver")
        {
            m_majorversion = payload.at(7);
            m_minorversion = payload.at(9);

            emit versionChanged(m_majorversion, m_minorversion);
        }
        else if(cmd == "InPr")
        {
            InputInfo info;
            info.index = (quint8)payload.at(6);
            info.longText = payload.mid(7, 20);
            info.shortText = payload.mid(27, 4);
            info.type = (quint8)payload.at(32); // 1 = SDI, 2 = HDMI, 32 = Internal (on TVS)
            m_inputInfos.insert(info.index, info);

            emit inputInfoChanged(info);
        }
        else if(cmd == "MPSE")
        {
            MediaInfo info;
            info.index = (quint8)payload.at(6);
            info.used = (quint8)payload.at(7);

            if(info.used)
            {
                info.name = payload.mid(8);
            }

            m_mediaInfos.insert(info.index, info);

            emit mediaInfoChanged(info);
        }
        else if(cmd == "MvIn")
        {
            quint8 mvindex = (quint8)payload.at(7);    // Index of multiview output
            quint8 inputindex = (quint8)payload.at(8); // Index of input, these are mapped in the InPr command
            m_multiViewInputs[mvindex] = inputindex;
        }
        else if(cmd == "MvPr")
        {
            m_multiViewLayoutIndex = (quint8)payload.at(7);
        }
        else if(cmd == "VidM")
        {
            m_videoFormatIndex = (quint8)payload.at(6);
        }
        else if(cmd == "Time")
        {
            U32_U8 val;
            val.u8[3] = (quint8)payload.at(6);
            val.u8[2] = (quint8)payload.at(7);
            val.u8[1] = (quint8)payload.at(8);
            val.u8[0] = (quint8)payload.at(9);

            emit timeChanged(val.u32);
        }
        else if(cmd == "TMxP")
        {
            m_mixFrames = (quint8)payload.at(7);

            emit mixFramesChanged(m_mixFrames);
        }
        else if(cmd == "TDpP")
        {
            m_dipFrames = (quint8)payload.at(7);

            emit dipFramesChanged(m_dipFrames);
        }
        else if(cmd == "TWpP")
        {
            m_wipeFrames = (quint8)payload.at(7);
            m_wipeType = (quint8)payload.at(8);

            U16_U8 val;
            val.u8[1] = (quint8)payload.at(10);
            val.u8[0] = (quint8)payload.at(11);
            m_wipeBorderWidth = val.u16;
            val.u8[1] = (quint8)payload.at(12);
            val.u8[0] = (quint8)payload.at(13);
            m_wipeSymmetry = val.u16;
            val.u8[1] = (quint8)payload.at(14);
            val.u8[0] = (quint8)payload.at(15);
            m_wipeBorderSoftness = val.u16;
            val.u8[1] = (quint8)payload.at(16);
            val.u8[0] = (quint8)payload.at(17);
            m_wipeXPosition = val.u16;
            val.u8[1] = (quint8)payload.at(18);
            val.u8[0] = (quint8)payload.at(19);
            m_wipeYPosition = val.u16;
            m_wipeReverseDirection = (quint8)payload.at(20);
            m_wipeFlipFlop = (quint8)payload.at(21);

            emit wipeFramesChanged(m_wipeFrames);
            emit wipeBorderWidthChanged(m_wipeBorderWidth);
            emit wipeBorderSoftnessChanged(m_wipeBorderSoftness);
            emit wipeTypeChanged(m_wipeType);
            emit wipeSymmetryChanged(m_wipeSymmetry);
            emit wipeXPositionChanged(m_wipeXPosition);
            emit wipeYPositionChanged(m_wipeYPosition);
            emit wipeReverseDirectionChanged(m_wipeReverseDirection);
            emit wipeFlipFlopChanged(m_wipeFlipFlop);
        }
        else if(cmd == "TDvP")
        {
            m_dveFrames = (quint8)payload.at(7);

            emit dveFramesChanged(m_dveFrames);
        }
        else if(cmd == "TStP")
        {
            m_stingFrames = (quint8)payload.at(7);

            emit stingFramesChanged(m_stingFrames);
        }
        else if(cmd == "BrdI")
        {
            m_borderSource = (quint8)payload.at(7);

            emit borderSourceChanged(m_borderSource);
        }
        else if(cmd == "KeBP")
        {
            quint8 index = (quint8)payload.at(6);
            m_upstreamKeyType[index] = (quint8)payload.at(8);
            m_upstreamKeyFillSource[index] = (quint8)payload.at(12);
            m_upstreamKeyKeySource[index] = (quint8)payload.at(13);
            m_upstreamKeyEnableMask[index] = (quint8)payload.at(14);
            U16_U8 val;
            val.u8[1] = (quint8)payload.at(16);
            val.u8[0] = (quint8)payload.at(17);
            m_upstreamKeyTopMask[index] = (qint16)val.u16;
            val.u8[1] = (quint8)payload.at(18);
            val.u8[0] = (quint8)payload.at(19);
            m_upstreamKeyBottomMask[index] = (qint16)val.u16;
            val.u8[1] = (quint8)payload.at(20);
            val.u8[0] = (quint8)payload.at(21);
            m_upstreamKeyLeftMask[index] = (qint16)val.u16;
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            m_upstreamKeyRightMask[index] = (qint16)val.u16;

            emit upstreamKeyTypeChanged(index, m_upstreamKeyType[index]);
            emit upstreamKeyFillSourceChanged(index, m_upstreamKeyFillSource[index]);
            emit upstreamKeyKeySourceChanged(index, m_upstreamKeyKeySource[index]);
            emit upstreamKeyEnableMaskChanged(index, m_upstreamKeyEnableMask[index]);
            emit upstreamKeyTopMaskChanged(index, m_upstreamKeyTopMask[index]);
            emit upstreamKeyBottomMaskChanged(index, m_upstreamKeyBottomMask[index]);
            emit upstreamKeyLeftMaskChanged(index, m_upstreamKeyLeftMask[index]);
            emit upstreamKeyRightMaskChanged(index, m_upstreamKeyRightMask[index]);
        }
        else if(cmd == "KeLm")
        {
            quint8 index = (quint8)payload.at(6);
            m_upstreamKeyLumaPreMultipliedKey[index] = (quint8)payload.at(8);
            U16_U8 val;
            val.u8[1] = (quint8)payload.at(10);
            val.u8[0] = (quint8)payload.at(11);
            m_upstreamKeyLumaClip[index] = val.u16;
            val.u8[1] = (quint8)payload.at(12);
            val.u8[0] = (quint8)payload.at(13);
            m_upstreamKeyLumaGain[index] = val.u16;
            m_upstreamKeyLumaInvertKey[index] = (quint8)payload.at(14);

            emit upstreamKeyLumaPreMultipliedKeyChanged(index, m_upstreamKeyLumaPreMultipliedKey[index]);
            emit upstreamKeyLumaClipChanged(index, m_upstreamKeyLumaClip[index]);
            emit upstreamKeyLumaGainChanged(index, m_upstreamKeyLumaGain[index]);
            emit upstreamKeyLumaInvertKeyChanged(index, m_upstreamKeyLumaInvertKey[index]);
        }
        else if(cmd == "KeCk")
        {
            quint8 index = (quint8)payload.at(6);
            U16_U8 val;
            val.u8[1] = (quint8)payload.at(8);
            val.u8[0] = (quint8)payload.at(9);
            m_upstreamKeyChromaHue[index] = val.u16;
            val.u8[1] = (quint8)payload.at(10);
            val.u8[0] = (quint8)payload.at(11);
            m_upstreamKeyChromaGain[index] = val.u16;
            val.u8[1] = (quint8)payload.at(12);
            val.u8[0] = (quint8)payload.at(13);
            m_upstreamKeyChromaYSuppress[index] = val.u16;
            val.u8[1] = (quint8)payload.at(14);
            val.u8[0] = (quint8)payload.at(15);
            m_upstreamKeyChromaLift[index] = val.u16;
            m_upstreamKeyChromaNarrowRange[index] = (quint8)payload.at(16);

            emit upstreamKeyChromaHueChanged(index, m_upstreamKeyChromaHue[index]);
            emit upstreamKeyChromaGainChanged(index, m_upstreamKeyChromaGain[index]);
            emit upstreamKeyChromaYSuppressChanged(index, m_upstreamKeyChromaYSuppress[index]);
            emit upstreamKeyChromaLiftChanged(index, m_upstreamKeyChromaLift[index]);
            emit upstreamKeyChromaNarrowRangeChanged(index, m_upstreamKeyChromaNarrowRange[index]);
        }
        else if(cmd == "KePt")
        {
            quint8 index = (quint8)payload.at(6);
            m_upstreamKeyPatternPattern[index] = (quint8)payload.at(8);
            U16_U8 val;
            val.u8[1] = (quint8)payload.at(10);
            val.u8[0] = (quint8)payload.at(11);
            m_upstreamKeyPatternSize[index] = val.u16;
            val.u8[1] = (quint8)payload.at(12);
            val.u8[0] = (quint8)payload.at(13);
            m_upstreamKeyPatternSymmetry[index] = val.u16;
            val.u8[1] = (quint8)payload.at(14);
            val.u8[0] = (quint8)payload.at(15);
            m_upstreamKeyPatternSoftness[index] = val.u16;
            val.u8[1] = (quint8)payload.at(16);
            val.u8[0] = (quint8)payload.at(17);
            m_upstreamKeyPatternXPosition[index] = val.u16;
            val.u8[1] = (quint8)payload.at(18);
            val.u8[0] = (quint8)payload.at(19);
            m_upstreamKeyPatternYPosition[index] = val.u16;
            m_upstreamKeyPatternInvertPattern[index] = (quint8)payload.at(20);

            emit upstreamKeyPatternPatternChanged(index, m_upstreamKeyPatternPattern[index]);
            emit upstreamKeyPatternSize(index, m_upstreamKeyPatternSize[index]);
            emit upstreamKeyPatternSymmetry(index, m_upstreamKeyPatternSymmetry[index]);
            emit upstreamKeyPatternSoftness(index, m_upstreamKeyPatternSoftness[index]);
            emit upstreamKeyPatternXPosition(index, m_upstreamKeyPatternXPosition[index]);
            emit upstreamKeyPatternYPosition(index, m_upstreamKeyPatternYPosition[index]);
            emit upstreamKeyPatternInvertPatternChanged(index, m_upstreamKeyPatternInvertPattern[index]);
        }
        else
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

void QAtemConnection::sendDatagram(const QByteArray& datagram)
{
    m_socket->writeDatagram(datagram, m_address, m_port);

//    qDebug() << "Data sent:" << datagram.toHex();
}

void QAtemConnection::sendCommand(const QByteArray& cmd, const QByteArray& payload)
{
    U16_U8 size;

    size.u16 = payload.size() + cmd.size() + 4;

    QByteArray datagram = createCommandHeader(Cmd_AckRequest, size.u16, m_currentUid, 0x0, 0x0, 0x0);

    datagram.append(size.u8[1]);
    datagram.append(size.u8[0]);

    datagram.append((char)0);
    datagram.append((char)0);

    datagram.append(cmd);
    datagram.append(payload);

    sendDatagram(datagram);
}

void QAtemConnection::handleError(QAbstractSocket::SocketError)
{
    emit socketError(m_socket->errorString());
}

void QAtemConnection::changeProgramInput(char index)
{
    QByteArray cmd = "CPgI";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append(index);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::changePreviewInput(char index)
{
    QByteArray cmd = "CPvI";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append(index);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::doCut()
{
    QByteArray cmd = "DCut";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0xef);
    payload.append((char)0xbf);
    payload.append((char)0x5f);

    sendCommand(cmd, payload);
}

void QAtemConnection::doAuto()
{
    QByteArray cmd = "DAut";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0x32);
    payload.append((char)0x16);
    payload.append((char)0x02);

    sendCommand(cmd, payload);
}

void QAtemConnection::toggleFadeToBlack()
{
    QByteArray cmd = "FtbA";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0x02);
    payload.append((char)0x58);
    payload.append((char)0x99);

    sendCommand(cmd, payload);
}

void QAtemConnection::setFadeToBlackFrameRate(quint8 frames)
{
    QByteArray cmd = "FtbC";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)0x00);
    payload.append((char)frames);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setTransitionPosition(quint16 position)
{
    QByteArray cmd = "CTPs";
    QByteArray payload;
    U16_U8 val;

    val.u16 = position;

    payload.append((char)0x00);
    payload.append((char)0xe4);
    payload.append(val.u8[1]);
    payload.append(val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::signalTransitionPositionChangeDone()
{
    QByteArray cmd = "CTPs";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0xf6);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setTransitionPreview(bool state)
{
    QByteArray cmd = "CTPr";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)state);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setTransitionType(quint8 type)
{
    QByteArray cmd = "CTTp";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)0x00);
    payload.append((char)type);
    payload.append((char)0x02);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyOn(quint8 keyer, bool state)
{
    QByteArray cmd = "CKOn";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)keyer);
    payload.append((char)state);
    payload.append((char)0x90);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyOnNextTransition(quint8 keyer, bool state)
{
    QByteArray cmd = "CTTp";
    QByteArray payload;

    quint8 stateValue = keyersOnNextTransition();

    if(state)
    {
        stateValue |= (0x2 << keyer);
    }
    else
    {
        stateValue &= (~(0x2 << keyer));
    }

    payload.append((char)0x02);
    payload.append((char)0x00);
    payload.append((char)0x6a);
    payload.append((char)stateValue & 0x1f);

    sendCommand(cmd, payload);
}

void QAtemConnection::setBackgroundOnNextTransition(bool state)
{
    QByteArray cmd = "CTTp";
    QByteArray payload;

    quint8 stateValue = keyersOnNextTransition();

    if(state)
    {
        stateValue |= 0x1;
    }
    else
    {
        stateValue &= (~0x1);
    }

    payload.append((char)0x02);
    payload.append((char)0x00);
    payload.append((char)0x71);
    payload.append((char)stateValue & 0x1f);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyOn(quint8 keyer, bool state)
{
    QByteArray cmd = "CDsL";
    QByteArray payload;

    payload.append((char)keyer);
    payload.append((char)state);
    payload.append((char)0xff);
    payload.append((char)0xff);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyTie(quint8 keyer, bool state)
{
    QByteArray cmd = "CDsT";
    QByteArray payload;

    payload.append((char)keyer);
    payload.append((char)state);
    payload.append((char)0xff);
    payload.append((char)0xff);

    sendCommand(cmd, payload);
}

void QAtemConnection::doDownstreamKeyAuto(quint8 keyer)
{
    QByteArray cmd = "DDsA";
    QByteArray payload;

    payload.append((char)keyer);
    payload.append((char)0x32);
    payload.append((char)0x16);
    payload.append((char)0x02);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyFillSource(quint8 keyer, quint8 source)
{
    QByteArray cmd = "CDsF";
    QByteArray payload;

    payload.append((char)keyer);
    payload.append((char)source);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyKeySource(quint8 keyer, quint8 source)
{
    QByteArray cmd = "CDsC";
    QByteArray payload;

    payload.append((char)keyer);
    payload.append((char)source);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyFrameRate(quint8 keyer, quint8 frames)
{
    QByteArray cmd = "CDsR";
    QByteArray payload;

    payload.append((char)keyer);
    payload.append((char)frames);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyInvertKey(quint8 keyer, bool invert)
{
    QByteArray cmd = "CDsG";
    QByteArray payload;

    payload.append((char)0x08);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)invert);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyPreMultiplied(quint8 keyer, bool preMultiplied)
{
    QByteArray cmd = "CDsG";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)keyer);
    payload.append((char)preMultiplied);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyClip(quint8 keyer, float clip)
{
    QByteArray cmd = "CDsG";
    QByteArray payload;
    U16_U8 val;
    val.u16 = clip * 10;

    payload.append((char)0x02);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyGain(quint8 keyer, float gain)
{
    QByteArray cmd = "CDsG";
    QByteArray payload;
    U16_U8 val;
    val.u16 = gain * 10;

    payload.append((char)0x04);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyEnableMask(quint8 keyer, bool enable)
{
    QByteArray cmd = "CDsM";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)keyer);
    payload.append((char)enable);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyTopMask(quint8 keyer, float value)
{
    QByteArray cmd = "CDsM";
    QByteArray payload;

    payload.append((char)0x1e);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    U16_U8 val;
    val.u16 = (quint16)value * 1000;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_bottomMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_leftMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_rightMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyBottomMask(quint8 keyer, float value)
{
    QByteArray cmd = "CDsM";
    QByteArray payload;

    payload.append((char)0x1e);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    U16_U8 val;
    val.u16 = (quint16)m_downstreamKey[keyer].m_topMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)value * 1000;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_leftMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_rightMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyLeftMask(quint8 keyer, float value)
{
    QByteArray cmd = "CDsM";
    QByteArray payload;

    payload.append((char)0x1e);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    U16_U8 val;
    val.u16 = (quint16)m_downstreamKey[keyer].m_topMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_bottomMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)value * 1000;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_rightMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDownstreamKeyRightMask(quint8 keyer, float value)
{
    QByteArray cmd = "CDsM";
    QByteArray payload;

    payload.append((char)0x1e);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    U16_U8 val;
    val.u16 = (quint16)m_downstreamKey[keyer].m_topMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_bottomMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_downstreamKey[keyer].m_leftMask;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)value * 1000;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::saveSettings()
{
    QByteArray cmd = "SRsv";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::clearSettings()
{
    QByteArray cmd = "SRcl";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setColorGeneratorColor(quint8 generator, const QColor& color)
{
    QByteArray cmd = "CClV";
    QByteArray payload;

    U16_U8 h, s, l;
    h.u16 = (color.hslHueF() * 360.0) * 10;
    s.u16 = color.hslSaturationF() * 1000;
    l.u16 = color.lightnessF() * 1000;

    payload.append((char)0x07);
    payload.append((char)generator);
    payload.append((char)h.u8[1]);
    payload.append((char)h.u8[0]);
    payload.append((char)s.u8[1]);
    payload.append((char)s.u8[0]);
    payload.append((char)l.u8[1]);
    payload.append((char)l.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::setMediaPlayerSource(quint8 player, bool clip, quint8 source)
{
    QByteArray cmd = "MPSS";
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

    payload.clear();
    payload.append((char)0x01);
    payload.append((char)player);
    payload.append((char)clip ? 2 : 1);
    payload.append((char)0xbf);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

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
    return m_downstreamKey.value(keyer).m_onAir;
}

bool QAtemConnection::downstreamKeyTie(quint8 keyer) const
{
    return m_downstreamKey.value(keyer).m_tie;
}

bool QAtemConnection::upstreamKeyOn(quint8 keyer) const
{
    return m_upstreamKeyOn.value(keyer);
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

quint8 QAtemConnection::auxSource(quint8 aux) const
{
    return m_auxSource.value(aux);
}

void QAtemConnection::setMixFrames(quint8 frames)
{
    QByteArray cmd = "CTMx";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)frames);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setDipFrames(quint8 frames)
{
    QByteArray cmd = "CTDp";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)frames);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setBorderSource(quint8 index)
{
    QByteArray cmd = "CBrI";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)index);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeFrames(quint8 frames)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0x01);
    payload.append((char)0x00);
    payload.append((char)frames);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeBorderWidth(quint16 width)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;
    U16_U8 val;
    val.u16 = width;

    payload.append((char)0x00);
    payload.append((char)0x04);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeBorderSoftness(quint16 softness)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;
    U16_U8 val;
    val.u16 = softness;

    payload.append((char)0x00);
    payload.append((char)0x10);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeType(quint8 type)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0x02);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)type);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeSymmetry(quint16 value)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;
    U16_U8 val;
    val.u16 = value;

    payload.append((char)0x00);
    payload.append((char)0x08);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeXPosition(quint16 value)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;
    U16_U8 val;
    val.u16 = value;

    payload.append((char)0x00);
    payload.append((char)0x08);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[0]);
    payload.append((char)val.u8[1]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeYPosition(quint16 value)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;
    U16_U8 val;
    val.u16 = value;

    payload.append((char)0x00);
    payload.append((char)0x40);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeReverseDirection(bool reverse)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;

    payload.append((char)0x00);
    payload.append((char)0x80);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)reverse);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setWipeFlipFlop(bool flipFlop)
{
    QByteArray cmd = "CTWp";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)flipFlop);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyType(quint8 keyer, quint8 type)
{
    QByteArray cmd = "CKTp";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)type);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyFillSource(quint8 keyer, quint8 source)
{
    QByteArray cmd = "CKeF";
    QByteArray payload;

    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)source);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyKeySource(quint8 keyer, quint8 source)
{
    QByteArray cmd = "CKeC";
    QByteArray payload;

    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)source);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyEnableMask(quint8 keyer, bool enable)
{
    QByteArray cmd = "CKMs";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)enable);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyTopMask(quint8 keyer, qint16 value)
{
    QByteArray cmd = "CKMs";
    QByteArray payload;
    U16_U8 val;

    payload.append((char)0x1e);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    val.u16 = (quint16)value;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyBottomMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyLeftMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyRightMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyBottomMask(quint8 keyer, qint16 value)
{
    QByteArray cmd = "CKMs";
    QByteArray payload;
    U16_U8 val;

    payload.append((char)0x1e);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    val.u16 = (quint16)m_upstreamKeyTopMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)value;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyLeftMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyRightMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLeftMask(quint8 keyer, qint16 value)
{
    QByteArray cmd = "CKMs";
    QByteArray payload;
    U16_U8 val;

    payload.append((char)0x1e);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    val.u16 = (quint16)m_upstreamKeyTopMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyBottomMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)value;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyRightMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyRightMask(quint8 keyer, qint16 value)
{
    QByteArray cmd = "CKMs";
    QByteArray payload;
    U16_U8 val;

    payload.append((char)0x1e);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    val.u16 = (quint16)m_upstreamKeyTopMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyBottomMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)m_upstreamKeyLeftMask[keyer];
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    val.u16 = (quint16)value;
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLumaPreMultipliedKey(quint8 keyer, bool preMultiplied)
{
    QByteArray cmd = "CKLm";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)preMultiplied);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLumaInvertKey(quint8 keyer, bool invert)
{
    QByteArray cmd = "CKLm";
    QByteArray payload;

    payload.append((char)0x08);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)invert);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLumaClip(quint8 keyer, quint16 clip)
{
    QByteArray cmd = "CKLm";
    QByteArray payload;
    U16_U8 val;
    val.u16 = clip;

    payload.append((char)0x02);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyLumaGain(quint8 keyer, quint16 gain)
{
    QByteArray cmd = "CKLm";
    QByteArray payload;
    U16_U8 val;
    val.u16 = gain;

    payload.append((char)0x04);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaHue(quint8 keyer, quint16 hue)
{
    QByteArray cmd = "CKCk";
    QByteArray payload;
    U16_U8 val;
    val.u16 = hue;

    payload.append((char)0x01);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaGain(quint8 keyer, quint16 hue)
{
    QByteArray cmd = "CKCk";
    QByteArray payload;
    U16_U8 val;
    val.u16 = hue;

    payload.append((char)0x02);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaYSuppress(quint8 keyer, quint16 ySuppress)
{
    QByteArray cmd = "CKCk";
    QByteArray payload;
    U16_U8 val;
    val.u16 = ySuppress;

    payload.append((char)0x04);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaLift(quint8 keyer, quint16 lift)
{
    QByteArray cmd = "CKCk";
    QByteArray payload;
    U16_U8 val;
    val.u16 = lift;

    payload.append((char)0x08);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyChromaNarrowRange(quint8 keyer, bool narrowRange)
{
    QByteArray cmd = "CKCk";
    QByteArray payload;

    payload.append((char)0x10);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)narrowRange);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternPattern(quint8 keyer, quint8 pattern)
{
    QByteArray cmd = "CKPt";
    QByteArray payload;

    payload.append((char)0x01);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)pattern);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternInvertPattern(quint8 keyer, bool invert)
{
    QByteArray cmd = "CKPt";
    QByteArray payload;

    payload.append((char)0x40);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)invert);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternSize(quint8 keyer, quint16 size)
{
    QByteArray cmd = "CKPt";
    QByteArray payload;
    U16_U8 val;
    val.u16 = size;

    payload.append((char)0x02);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternSymmetry(quint8 keyer, quint16 symmetry)
{
    QByteArray cmd = "CKPt";
    QByteArray payload;
    U16_U8 val;
    val.u16 = symmetry;

    payload.append((char)0x04);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternSoftness(quint8 keyer, quint16 softness)
{
    QByteArray cmd = "CKPt";
    QByteArray payload;
    U16_U8 val;
    val.u16 = softness;

    payload.append((char)0x08);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternXPosition(quint8 keyer, quint16 xPosition)
{
    QByteArray cmd = "CKPt";
    QByteArray payload;
    U16_U8 val;
    val.u16 = xPosition;

    payload.append((char)0x10);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setUpstreamKeyPatternYPosition(quint8 keyer, quint16 yPosition)
{
    QByteArray cmd = "CKPt";
    QByteArray payload;
    U16_U8 val;
    val.u16 = yPosition;

    payload.append((char)0x20);
    payload.append((char)keyer);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)val.u8[1]);
    payload.append((char)val.u8[0]);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setAuxSource(quint8 aux, quint8 source)
{
    QByteArray cmd = "CAuS";
    QByteArray payload;

    payload.append((char)aux);
    payload.append((char)source);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setInputType(quint8 input, quint8 type)
{
    QByteArray cmd = "CInL";
    QByteArray payload;

    payload.append((char)0x04);
    payload.append((char)input);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)type);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setInputLongName(quint8 input, const QString &name)
{
    QByteArray cmd = "CInL";
    QByteArray payload;
    QByteArray namearray = name.toLatin1();
    namearray.resize(20);

    payload.append((char)0x01);
    payload.append((char)input);
    payload.append(namearray);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}

void QAtemConnection::setInputShortName(quint8 input, const QString &name)
{
    QByteArray cmd = "CInL";
    QByteArray payload;
    QByteArray namearray = name.toLatin1();
    namearray.resize(4);

    payload.append((char)0x02);
    payload.append((char)input);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append((char)0x00);
    payload.append(namearray);
    payload.append((char)0x00);
    payload.append((char)0x00);

    sendCommand(cmd, payload);
}
