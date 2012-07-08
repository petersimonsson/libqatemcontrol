#ifndef QDOWNSTREAMKEYSETTINGS_H
#define QDOWNSTREAMKEYSETTINGS_H

#include <QtGlobal>

struct QDownstreamKeySettings
{
    QDownstreamKeySettings ()
    {
        m_onAir = false;
        m_tie = false;
        m_frames = 0;
        m_frameCount = 0;
        m_fillSource = 0;
        m_keySource = 0;
        m_invertKey = false;
        m_preMultiplied = false;
        m_clip = 0;
        m_gain = 0;
        m_enableMask = false;
        m_topMask = 0;
        m_bottomMask = 0;
        m_leftMask = 0;
        m_rightMask = 0;
    }

    bool m_onAir;
    bool m_tie;
    quint8 m_frames;
    quint8 m_frameCount;
    quint8 m_fillSource;
    quint8 m_keySource;
    bool m_invertKey;
    bool m_preMultiplied;
    float m_clip;
    float m_gain;
    bool m_enableMask;
    float m_topMask;
    float m_bottomMask;
    float m_leftMask;
    float m_rightMask;
};

#endif //QDOWNSTREAMKEYSETTINGS_H
