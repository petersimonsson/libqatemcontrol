#ifndef QATEM_TOPOLOGY_H
#define QATEM_TOPOLOGY_H

#include <QtGlobal>

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
}

#endif //QATEM_TOPOLOGY_H
