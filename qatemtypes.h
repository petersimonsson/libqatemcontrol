/*
Copyright 2014  Peter Simonsson <peter.simonsson@gmail.com>

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

#ifndef QATEM_TOPOLOGY_H
#define QATEM_TOPOLOGY_H

#include <QtGlobal>
#include <QString>

namespace QAtem
{
    typedef union
    {
        quint16 u16;
        quint8 u8[2];
    } U16_U8;

    typedef union
    {
        quint32 u32;
        quint8 u8[4];
    } U32_U8;

    struct Topology
    {
        quint8 MEs;
        quint8 sources;
        quint8 colorGenerators;
        quint8 auxBusses;
        quint8 downstreamKeyers;
        quint8 stingers;
        quint8 DVEs;
        quint8 supersources;
        bool hasSD;
    };

    struct VideoMode
    {
        VideoMode(quint8 i, const QString &n) : index(i), name(n) {}

        quint8 index;
        QString name;
    };

    struct MultiView
    {
        MultiView(quint8 i) : index(i) {}

        quint8 index;
        /// Multi view layout, 0 = prg/prv on top, 1 = prg/prv on bottom, 2 = prg/prv on left, 3 = prg/prv on right
        quint8 layout;
        quint16 sources[10];
    };

    struct InputInfo
    {
        InputInfo()
        {
            index = 0;
            tally = 0;
            externalType = 0;
            internalType = 0;
            availableExternalTypes = 0;
            availability = 0;
            meAvailability = 0;
        }

        quint16 index;
        quint8 tally;
        quint8 externalType; // 0 = Internal, 1 = SDI, 2 = HDMI, 3 = Composite, 4 = Component, 5 = SVideo
        quint8 internalType; // 0 = External, 1 = Black, 2 = Color Bars, 3 = Color Generator, 4 = Media Player Fill, 5 = Media Player Key, 6 = SuperSource, 128 = ME Output, 129 = Auxiliary, 130 = Mask
        quint8 availableExternalTypes; // Bit 0: SDI, 1: HDMI, 2: Component, 3: Composite, 4: SVideo
        quint8 availability; // Bit 0: Auxiliary, 1: Multiviewer, 2: SuperSource Art, 3: SuperSource Box, 4: Key Sources
        quint8 meAvailability; // Bit 0: ME1 + Fill Sources, 1: ME2 + Fill Sources
        QString longText;
        QString shortText;
    };

    enum MediaType
    {
        StillMedia = 1,
        ClipMedia = 2
    };

    struct MediaInfo
    {
        quint8 index;
        bool used;
        quint8 frameCount;
        QString name;
        MediaType type;
        QByteArray hash;
    };

    struct MediaPlayerState
    {
        quint8 index;
        bool loop;
        bool playing;
        bool atBegining;
        quint8 currentFrame;
    };

    struct AudioInput
    {
        quint16 index;
        quint8 type; // 0 = Video input, 1 = Media player, 2 = External
        quint8 plugType; // 0 = Internal, 1 = SDI, 2 = HDMI, 3 = Component, 4 = Composite, 5 = SVideo, 32 = XLR, 64 = AES/EBU, 128 = RCA
        quint8 state; // 0 = Off, 1 = On, 2 = AFV
        float balance;
        float gain; // dB
    };

    struct AudioLevel
    {
        quint8 index;
        float left;
        float right;
        float peakLeft;
        float peakRight;
    };
}

#endif //QATEM_TOPOLOGY_H