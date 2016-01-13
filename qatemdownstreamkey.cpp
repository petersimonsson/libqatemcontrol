/*
Copyright 2015  Peter Simonsson <peter.simonsson@gmail.com>

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

#include "qatemdownstreamkey.h"
#include "qatemconnection.h"

QAtemDownstreamKey::QAtemDownstreamKey(quint8 id, QAtemConnection *parent) :
    QObject(parent), m_id (id), m_atemConnection(parent)
{
    m_onAir = false;
    m_tie = false;
    m_frameRate = 0;
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

    m_atemConnection->registerCommand("DskS", this, "onDskS");
    m_atemConnection->registerCommand("DskP", this, "onDskP");
    m_atemConnection->registerCommand("DskB", this, "onDskB");
}

QAtemDownstreamKey::~QAtemDownstreamKey()
{
    m_atemConnection->unregisterCommand("DskS", this);
    m_atemConnection->unregisterCommand("DskP", this);
    m_atemConnection->unregisterCommand("DskB", this);
}

void QAtemDownstreamKey::setOnAir(bool state)
{
    if(state == m_onAir)
    {
        return;
    }

    QByteArray cmd("CDsL");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;
    payload[1] = (char)state;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setTie(bool state)
{
    if(state == m_tie)
    {
        return;
    }

    QByteArray cmd("CDsT");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;
    payload[1] = (char)state;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::doAuto()
{
    QByteArray cmd("DDsA");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setFillSource(quint16 source)
{
    if(source == m_fillSource)
    {
        return;
    }

    QByteArray cmd("CDsF");
    QByteArray payload(4, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)m_id;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setKeySource(quint16 source)
{
    if(source == m_keySource)
    {
        return;
    }

    QByteArray cmd("CDsC");
    QByteArray payload(4, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)m_id;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setFrameRate(quint8 frames)
{
    if(frames == m_frameRate)
    {
        return;
    }

    QByteArray cmd("CDsR");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;
    payload[1] = (char)frames;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setInvertKey(bool invert)
{
    if(invert == m_invertKey)
    {
        return;
    }

    QByteArray cmd("CDsG");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x08;
    payload[1] = (char)m_id;
    payload[8] = (char)invert;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setPreMultiplied(bool preMultiplied)
{
    if(preMultiplied == m_preMultiplied)
    {
        return;
    }

    QByteArray cmd("CDsG");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)preMultiplied;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setClip(float clip)
{
    if(clip == m_clip)
    {
        return;
    }

    QByteArray cmd("CDsG");
    QByteArray payload(12, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = clip * 10;

    payload[0] = (char)0x02;
    payload[1] = (char)m_id;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setGain(float gain)
{
    if(gain == m_gain)
    {
        return;
    }

    QByteArray cmd("CDsG");
    QByteArray payload(12, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = gain * 10;

    payload[0] = (char)0x04;
    payload[1] = (char)m_id;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setEnableMask(bool enable)
{
    if(enable == m_enableMask)
    {
        return;
    }

    QByteArray cmd("CDsM");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)enable;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::setMask(float top, float bottom, float left, float right)
{
    QByteArray cmd("CDsM");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x1e;
    payload[1] = (char)m_id;
    QAtem::U16_U8 val;
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

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemDownstreamKey::onDskS(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);

    if(index == m_id)
    {
        m_onAir = (quint8)payload.at(7);
        m_frameCount = (quint8)payload.at(10);

        emit onAirChanged(m_id, m_onAir);
        emit frameCountChanged(m_id, m_frameCount);
    }
}

void QAtemDownstreamKey::onDskP(const QByteArray& payload)
{
    quint8 index = (quint8)payload.at(6);

    if(index == m_id)
    {
        m_tie = (quint8)payload.at(7);
        m_frameRate = (quint8)payload.at(8);
        m_preMultiplied = (quint8)payload.at(9);
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(10);
        val.u8[0] = (quint8)payload.at(11);
        m_clip = val.u16 / 10.0;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        m_gain = val.u16 / 10.0;
        m_invertKey = (quint8)payload.at(14);
        m_enableMask = (quint8)payload.at(15);
        val.u8[1] = (quint8)payload.at(16);
        val.u8[0] = (quint8)payload.at(17);
        m_topMask = (qint16)val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(18);
        val.u8[0] = (quint8)payload.at(19);
        m_bottomMask = (qint16)val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(20);
        val.u8[0] = (quint8)payload.at(21);
        m_leftMask = (qint16)val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(22);
        val.u8[0] = (quint8)payload.at(23);
        m_rightMask = (qint16)val.u16 / 1000.0;

        emit tieChanged(m_id, m_tie);
        emit frameRateChanged(m_id, m_frameRate);
        emit invertKeyChanged(m_id, m_invertKey);
        emit preMultipliedChanged(m_id, m_preMultiplied);
        emit clipChanged(m_id, m_clip);
        emit gainChanged(m_id, m_gain);
        emit enableMaskChanged(m_id, m_enableMask);
        emit maskChanged(m_id, m_topMask, m_bottomMask, m_leftMask, m_rightMask);
    }
}

void QAtemDownstreamKey::onDskB(const QByteArray& payload)
{
    QAtem::U16_U8 val;
    quint8 index = (quint8)payload.at(6);

    if(index == m_id)
    {
        val.u8[1] = (quint8)payload.at(8);
        val.u8[0] = (quint8)payload.at(9);
        m_fillSource = val.u16;
        val.u8[1] = (quint8)payload.at(10);
        val.u8[0] = (quint8)payload.at(11);
        m_keySource = val.u16;

        emit sourcesChanged(m_id, m_fillSource, m_keySource);
    }
}
