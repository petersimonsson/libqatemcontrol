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
}

#endif //QATEM_TOPOLOGY_H
