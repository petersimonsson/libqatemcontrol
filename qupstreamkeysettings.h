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

#ifndef QUPSTREAMKEYSETTINGS_H
#define QUPSTREAMKEYSETTINGS_H

struct QUpstreamKeySettings
{
    QUpstreamKeySettings ()
    {
        m_onAir = false;
        m_type = 0;
        m_fillSource = 0;
        m_keySource = 0;
        m_enableMask = false;
        m_topMask = 0;
        m_bottomMask = 0;
        m_leftMask = 0;
        m_rightMask = 0;
        m_lumaPreMultipliedKey = false;
        m_lumaInvertKey = false;
        m_lumaClip = 0;
        m_lumaGain = 0;
        m_chromaHue = 0;
        m_chromaGain = 0;
        m_chromaYSuppress = 0;
        m_chromaLift = 0;
        m_chromaNarrowRange = false;
        m_patternPattern = 0;
        m_patternInvertPattern = false;
        m_patternSize = 0;
        m_patternSymmetry = 0;
        m_patternSoftness = 0;
        m_patternXPosition = 0;
        m_patternYPosition = 0;
    }

    bool m_onAir;
    quint8 m_type;
    quint8 m_fillSource;
    quint8 m_keySource;
    bool m_enableMask;
    float m_topMask;
    float m_bottomMask;
    float m_leftMask;
    float m_rightMask;
    bool m_lumaPreMultipliedKey;
    bool m_lumaInvertKey;
    float m_lumaClip;
    float m_lumaGain;
    float m_chromaHue;
    float m_chromaGain;
    float m_chromaYSuppress;
    float m_chromaLift;
    bool m_chromaNarrowRange;
    quint8 m_patternPattern;
    bool m_patternInvertPattern;
    float m_patternSize;
    float m_patternSymmetry;
    float m_patternSoftness;
    float m_patternXPosition;
    float m_patternYPosition;
};

#endif // QUPSTREAMKEYSETTINGS_H
