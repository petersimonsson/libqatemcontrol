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
}

#endif //QATEM_TOPOLOGY_H
