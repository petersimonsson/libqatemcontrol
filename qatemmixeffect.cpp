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

#include "qatemmixeffect.h"

#include <QColor>

QAtemMixEffect::QAtemMixEffect(quint8 id, QAtemConnection *parent) :
    QObject(parent), m_id(id), m_atemConnection(parent)
{
    m_programInput = 0;
    m_previewInput = 0;

    m_transitionPreviewEnabled = false;
    m_transitionFrameCount = 0;
    m_transitionPosition = 0;
    m_keyersOnCurrentTransition = 0;
    m_currentTransitionStyle = 0;
    m_keyersOnNextTransition = 0;
    m_nextTransitionStyle = 0;

    m_fadeToBlackEnabled = false;
    m_fadeToBlackFading = false;
    m_fadeToBlackFrameCount = 0;
    m_fadeToBlackFrames = 0;

    m_mixFrames  = 0;

    m_dipFrames = 0;
    m_dipSource = 0;

    m_wipeFrames = 0;
    m_wipeBorderSource = 0;
    m_wipeBorderWidth = 0;
    m_wipeBorderSoftness = 0;
    m_wipeType = 0;
    m_wipeSymmetry = 0;
    m_wipeXPosition = 0;
    m_wipeYPosition = 0;
    m_wipeReverseDirection = false;
    m_wipeFlipFlop = false;

    m_dveRate = 0;
    m_dveEffect = 0;
    m_dveFillSource = 0;
    m_dveKeySource = 0;
    m_dveKeyEnabled = false;
    m_dvePreMultipliedKeyEnabled = false;
    m_dveKeyClip = 0;
    m_dveKeyGain = 0;
    m_dveEnableInvertKey = false;
    m_dveReverseDirection = false;
    m_dveFlipFlopDirection = false;

    m_stingerSource = 0;
    m_stingerPreMultipliedKeyEnabled = false;
    m_stingerClip = 0;
    m_stingerGain = 0;
    m_stingerInvertKeyEnabled = false;
    m_stingerPreRoll = 0;
    m_stingerClipDuration = 0;
    m_stingerTriggerPoint = 0;
    m_stingerMixRate = 0;

    m_atemConnection->registerCommand("PrgI", this, "onPrgI");
    m_atemConnection->registerCommand("PrvI", this, "onPrvI");

    m_atemConnection->registerCommand("TrPr", this, "onTrPr");
    m_atemConnection->registerCommand("TrPs", this, "onTrPs");
    m_atemConnection->registerCommand("TrSS", this, "onTrSS");

    m_atemConnection->registerCommand("FtbS", this, "onFtbS");
    m_atemConnection->registerCommand("FtbP", this, "onFtbP");

    m_atemConnection->registerCommand("TMxP", this, "onTMxP");

    m_atemConnection->registerCommand("TDpP", this, "onTDpP");

    m_atemConnection->registerCommand("TWpP", this, "onTWpP");

    m_atemConnection->registerCommand("TDvP", this, "onTDvP");

    m_atemConnection->registerCommand("TStP", this, "onTStP");

    m_atemConnection->registerCommand("KeOn", this, "onKeOn");
    m_atemConnection->registerCommand("KeBP", this, "onKeBP");
    m_atemConnection->registerCommand("KeLm", this, "onKeLm");
    m_atemConnection->registerCommand("KeCk", this, "onKeCk");
    m_atemConnection->registerCommand("KePt", this, "onKePt");
    m_atemConnection->registerCommand("KeDV", this, "onKeDV");
    m_atemConnection->registerCommand("KeFS", this, "onKeFS");
    m_atemConnection->registerCommand("KKFP", this, "onKKFP");
}

QAtemMixEffect::~QAtemMixEffect()
{
    m_atemConnection->unregisterCommand("PrgI", this);
    m_atemConnection->unregisterCommand("PrvI", this);

    m_atemConnection->unregisterCommand("TrPr", this);
    m_atemConnection->unregisterCommand("TrPs", this);
    m_atemConnection->unregisterCommand("TrSS", this);

    m_atemConnection->unregisterCommand("FtbS", this);
    m_atemConnection->unregisterCommand("FtbP", this);

    m_atemConnection->unregisterCommand("TMxP", this);

    m_atemConnection->unregisterCommand("TDpP", this);

    m_atemConnection->unregisterCommand("TWpP", this);

    m_atemConnection->unregisterCommand("TDvP", this);

    m_atemConnection->unregisterCommand("TStP", this);

    m_atemConnection->unregisterCommand("KeOn", this);
    m_atemConnection->unregisterCommand("KeBP", this);
    m_atemConnection->unregisterCommand("KeLm", this);
    m_atemConnection->unregisterCommand("KeCk", this);
    m_atemConnection->unregisterCommand("KePt", this);
    m_atemConnection->unregisterCommand("KeDV", this);
    m_atemConnection->unregisterCommand("KeFS", this);
    m_atemConnection->unregisterCommand("KKFP", this);

    qDeleteAll(m_upstreamKeys);
}

void QAtemMixEffect::createUpstreamKeyers(quint8 count)
{
    qDeleteAll(m_upstreamKeys);
    m_upstreamKeys.resize(count);

    for(int i = 0; i < m_upstreamKeys.count(); ++i)
    {
        QUpstreamKeySettings *key = new QUpstreamKeySettings(i);
        m_upstreamKeys[i] = key;
    }
}

void QAtemMixEffect::cut()
{
    QByteArray cmd("DCut");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::autoTransition()
{
    QByteArray cmd("DAut");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::changeProgramInput(quint16 index)
{
    if(index == m_programInput)
    {
        return;
    }

    QByteArray cmd("CPgI");
    QByteArray payload(4, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)m_id;
    val.u16 = index;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setTransitionPosition(quint16 position)
{
    if(position == m_transitionPosition)
    {
        return;
    }

    QByteArray cmd("CTPs");
    QByteArray payload(4, (char)0x0);
    QAtem::U16_U8 val;

    val.u16 = position;

    payload[0] = (char)m_id;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::changePreviewInput(quint16 index)
{
    if(index == m_previewInput)
    {
        return;
    }

    QByteArray cmd("CPvI");
    QByteArray payload(4, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)m_id;
    val.u16 = index;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setTransitionPreview(bool state)
{
    if(state == m_transitionPreviewEnabled)
    {
        return;
    }

    QByteArray cmd("CTPr");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;
    payload[1] = (char)state;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setTransitionType(quint8 type)
{
    QByteArray cmd("CTTp");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)type;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyOnNextTransition(quint8 keyer, bool state)
{
    setKeyOnNextTransition(keyer + 1, state);
}

void QAtemMixEffect::setBackgroundOnNextTransition(bool state)
{
    setKeyOnNextTransition(0, state);
}

void QAtemMixEffect::setKeyOnNextTransition (int index, bool state)
{
    QByteArray cmd("CTTp");
    QByteArray payload(4, (char)0x0);

    quint8 stateValue = keyersOnNextTransition();

    if(state)
    {
        stateValue |= (0x1 << index);
    }
    else
    {
        stateValue &= (~(0x1 << index));
    }

    if(stateValue == keyersOnNextTransition())
    {
        return;
    }
    else if(stateValue == 0)
    {
        emit keyersOnNextTransitionChanged(m_id, keyersOnNextTransition());
        return;
    }

    payload[0] = (char)0x02;
    payload[1] = (char)m_id;
    payload[3] = (char)stateValue & 0x1f;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::toggleFadeToBlack()
{
    QByteArray cmd("FtbA");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;
    payload[1] = (char)0x02; // Does not toggle without this set

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setFadeToBlackFrameRate(quint8 frames)
{
    if(frames == m_fadeToBlackFrameCount)
    {
        return;
    }

    QByteArray cmd("FtbC");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)frames;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setMixFrames(quint8 frames)
{
    QByteArray cmd("CTMx");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;
    payload[1] = (char)frames;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDipFrames(quint8 frames)
{
    QByteArray cmd("CTDp");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)frames;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDipSource(quint16 source)
{
    QByteArray cmd("CTDp");
    QByteArray payload(8, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)0x02;
    payload[1] = (char)m_id;
    val.u16 = source;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeBorderSource(quint16 source)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;

    payload[1] = (char)0x08;
    payload[2] = (char)m_id;
    val.u16 = source;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeFrames(quint8 frames)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x01;
    payload[2] = (char)m_id;
    payload[3] = (char)frames;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeBorderWidth(quint16 width)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = width;

    payload[1] = (char)0x04;
    payload[2] = (char)m_id;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeBorderSoftness(quint16 softness)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = softness;

    payload[1] = (char)0x20;
    payload[2] = (char)m_id;
    payload[12] = (char)val.u8[1];
    payload[13] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeType(quint8 type)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x02;
    payload[2] = (char)m_id;
    payload[4] = (char)type;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeSymmetry(quint16 value)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = value;

    payload[1] = (char)0x10;
    payload[2] = (char)m_id;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeXPosition(quint16 value)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = value;

    payload[1] = (char)0x40;
    payload[2] = (char)m_id;
    payload[14] = (char)val.u8[1];
    payload[15] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeYPosition(quint16 value)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = value;

    payload[1] = (char)0x80;
    payload[2] = (char)m_id;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeReverseDirection(bool reverse)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x01;
    payload[2] = (char)m_id;
    payload[18] = (char)reverse;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setWipeFlipFlop(bool flipFlop)
{
    QByteArray cmd("CTWp");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x02;
    payload[2] = (char)m_id;
    payload[19] = (char)flipFlop;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVERate(quint8 frames)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = frames;

    payload[1] = (char)0x01;
    payload[2] = (char)m_id;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEEffect(quint8 effect)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x04;
    payload[2] = (char)m_id;
    payload[5] = (char)effect;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEFillSource(quint16 source)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;

    payload[1] = (char)0x08;
    payload[2] = (char)m_id;
    val.u16 = source;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEKeySource(quint16 source)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;

    payload[1] = (char)0x10;
    payload[2] = (char)m_id;
    val.u16 = source;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEKeyEnabled(bool enabled)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x20;
    payload[2] = (char)m_id;
    payload[10] = (char)enabled;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEPreMultipliedKeyEnabled(bool enabled)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x40;
    payload[2] = (char)m_id;
    payload[11] = (char)enabled;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEKeyClip(float percent)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = percent * 10;

    payload[1] = (char)0x80;
    payload[2] = (char)m_id;
    payload[12] = (char)val.u8[1];
    payload[13] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEKeyGain(float percent)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = percent * 10;

    payload[0] = (char)0x01;
    payload[2] = (char)m_id;
    payload[14] = (char)val.u8[1];
    payload[15] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEInvertKeyEnabled(bool enabled)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x02;
    payload[2] = (char)m_id;
    payload[16] = (char)enabled;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEReverseDirection(bool reverse)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x04;
    payload[2] = (char)m_id;
    payload[17] = (char)reverse;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setDVEFlipFlopDirection(bool flipFlop)
{
    QByteArray cmd("CTDv");
    QByteArray payload(20, (char)0x0);

    payload[0] = (char)0x08;
    payload[2] = (char)m_id;
    payload[18] = (char)flipFlop;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerSource(quint8 source)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x01;
    payload[2] = (char)m_id;
    payload[3] = (char)source;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerPreMultipliedKeyEnabled(bool enabled)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x02;
    payload[2] = (char)m_id;
    payload[4] = (char)enabled;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerClip(float percent)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = percent * 10;

    payload[1] = (char)0x04;
    payload[2] = (char)m_id;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerGain(float percent)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = percent * 10;

    payload[1] = (char)0x08;
    payload[2] = (char)m_id;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerInvertKeyEnabled(bool enabled)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);

    payload[1] = (char)0x10;
    payload[2] = (char)m_id;
    payload[10] = (char)enabled;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerPreRoll(quint16 frames)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = frames;

    payload[1] = (char)0x20;
    payload[2] = (char)m_id;
    payload[12] = (char)val.u8[1];
    payload[13] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerClipDuration(quint16 frames)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = frames;

    payload[1] = (char)0x40;
    payload[2] = (char)m_id;
    payload[14] = (char)val.u8[1];
    payload[15] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerTriggerPoint(quint16 frames)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = frames;

    payload[1] = (char)0x80;
    payload[2] = (char)m_id;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setStingerMixRate(quint16 frames)
{
    QByteArray cmd("CTSt");
    QByteArray payload(20, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = frames;

    payload[0] = (char)0x01;
    payload[2] = (char)m_id;
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyOnAir(quint8 keyer, bool state)
{
    if(state == m_upstreamKeys[keyer]->m_onAir)
    {
        return;
    }

    QByteArray cmd("CKOn");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;
    payload[1] = (char)keyer;
    payload[2] = (char)state;

    m_atemConnection->sendCommand(cmd, payload);
}

bool QAtemMixEffect::upstreamKeyOnAir(quint8 keyer) const
{
    if(keyer < m_upstreamKeys.count())
    {
        return m_upstreamKeys[keyer]->m_onAir;
    }
    else
    {
        return false;
    }
}

void QAtemMixEffect::setUpstreamKeyType(quint8 keyer, quint8 type)
{
    if(keyer >= m_upstreamKeys.count() || type == m_upstreamKeys[keyer]->m_type)
    {
        return;
    }

    QByteArray cmd("CKTp");
    QByteArray payload(10, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[3] = (char)type;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyFillSource(quint8 keyer, quint16 source)
{
    if(keyer >= m_upstreamKeys.count() || source == m_upstreamKeys[keyer]->m_fillSource)
    {
        return;
    }

    QByteArray cmd("CKeF");
    QByteArray payload(4, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)m_id;
    payload[1] = (char)keyer;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyKeySource(quint8 keyer, quint16 source)
{
    if(keyer >= m_upstreamKeys.count() || source == m_upstreamKeys[keyer]->m_keySource)
    {
        return;
    }

    QByteArray cmd("CKeC");
    QByteArray payload(4, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)m_id;
    payload[1] = (char)keyer;
    val.u16 = source;
    payload[2] = (char)val.u8[1];
    payload[3] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyEnableMask(quint8 keyer, bool enable)
{
    if(keyer >= m_upstreamKeys.count() || enable == m_upstreamKeys[keyer]->m_enableMask)
    {
        return;
    }

    QByteArray cmd("CKMs");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[3] = (char)enable;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyMask(quint8 keyer, float top, float bottom, float left, float right)
{
    QByteArray cmd("CKMs");
    QByteArray payload(12, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)0x1e;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
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

void QAtemMixEffect::setUpstreamKeyLumaPreMultipliedKey(quint8 keyer, bool preMultiplied)
{
    if(keyer >= m_upstreamKeys.count() || preMultiplied == m_upstreamKeys[keyer]->m_lumaPreMultipliedKey)
    {
        return;
    }

    QByteArray cmd("CKLm");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[3] = (char)preMultiplied;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyLumaInvertKey(quint8 keyer, bool invert)
{
    if(keyer >= m_upstreamKeys.count() || invert == m_upstreamKeys[keyer]->m_lumaInvertKey)
    {
        return;
    }

    QByteArray cmd("CKLm");
    QByteArray payload(12, (char)0x0);

    payload[0] = (char)0x08;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[8] = (char)invert;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyLumaClip(quint8 keyer, float clip)
{
    if(keyer >= m_upstreamKeys.count() || clip == m_upstreamKeys[keyer]->m_lumaClip)
    {
        return;
    }

    QByteArray cmd("CKLm");
    QByteArray payload(12, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = clip * 10;

    payload[0] = (char)0x02;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyLumaGain(quint8 keyer, float gain)
{
    if(keyer >= m_upstreamKeys.count() || gain == m_upstreamKeys[keyer]->m_lumaGain)
    {
        return;
    }

    QByteArray cmd("CKLm");
    QByteArray payload(12, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = gain * 10;

    payload[0] = (char)0x04;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyChromaHue(quint8 keyer, float hue)
{
    if(keyer >= m_upstreamKeys.count() || hue == m_upstreamKeys[keyer]->m_chromaHue)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = hue * 10;

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyChromaGain(quint8 keyer, float gain)
{
    if(keyer >= m_upstreamKeys.count() || gain == m_upstreamKeys[keyer]->m_chromaGain)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = gain * 10;

    payload[0] = (char)0x02;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyChromaYSuppress(quint8 keyer, float ySuppress)
{
    if(keyer >= m_upstreamKeys.count() || ySuppress == m_upstreamKeys[keyer]->m_chromaYSuppress)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = ySuppress * 10;

    payload[0] = (char)0x04;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyChromaLift(quint8 keyer, float lift)
{
    if(keyer >= m_upstreamKeys.count() || lift == m_upstreamKeys[keyer]->m_chromaLift)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = lift * 10;

    payload[0] = (char)0x08;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyChromaNarrowRange(quint8 keyer, bool narrowRange)
{
    if(keyer >= m_upstreamKeys.count() || narrowRange == m_upstreamKeys[keyer]->m_chromaNarrowRange)
    {
        return;
    }

    QByteArray cmd("CKCk");
    QByteArray payload(16, (char)0x0);

    payload[0] = (char)0x10;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[12] = (char)narrowRange;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyPatternPattern(quint8 keyer, quint8 pattern)
{
    if(keyer >= m_upstreamKeys.count() || pattern == m_upstreamKeys[keyer]->m_patternPattern)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);

    payload[0] = (char)0x01;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[3] = (char)pattern;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyPatternInvertPattern(quint8 keyer, bool invert)
{
    if(keyer >= m_upstreamKeys.count() || invert == m_upstreamKeys[keyer]->m_patternInvertPattern)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);

    payload[0] = (char)0x40;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[14] = (char)invert;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyPatternSize(quint8 keyer, float size)
{
    if(keyer >= m_upstreamKeys.count() || size == m_upstreamKeys[keyer]->m_patternSize)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = size * 100;

    payload[0] = (char)0x02;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[4] = (char)val.u8[1];
    payload[5] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyPatternSymmetry(quint8 keyer, float symmetry)
{
    if(keyer >= m_upstreamKeys.count() || symmetry == m_upstreamKeys[keyer]->m_patternSymmetry)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = symmetry * 100;

    payload[0] = (char)0x04;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[6] = (char)val.u8[1];
    payload[7] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyPatternSoftness(quint8 keyer, float softness)
{
    if(keyer >= m_upstreamKeys.count() || softness == m_upstreamKeys[keyer]->m_patternSoftness)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = softness * 100;

    payload[0] = (char)0x08;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[8] = (char)val.u8[1];
    payload[9] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyPatternXPosition(quint8 keyer, float xPosition)
{
    if(keyer >= m_upstreamKeys.count() || xPosition == m_upstreamKeys[keyer]->m_patternXPosition)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = xPosition * 1000;

    payload[0] = (char)0x10;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyPatternYPosition(quint8 keyer, float yPosition)
{
    if(keyer >= m_upstreamKeys.count() || yPosition == m_upstreamKeys[keyer]->m_patternYPosition)
    {
        return;
    }

    QByteArray cmd("CKPt");
    QByteArray payload(16, (char)0x0);
    QAtem::U16_U8 val;
    val.u16 = yPosition * 1000;

    payload[0] = (char)0x20;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[12] = (char)val.u8[1];
    payload[13] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEPosition(quint8 keyer, float xPosition, float yPosition)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[3] = (char)0x0c;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    val.u16 = xPosition * 1000;
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];
    val.u16 = yPosition * 1000;
    payload[22] = (char)val.u8[1];
    payload[23] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVESize(quint8 keyer, float xSize, float ySize)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[3] = (char)0x03;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    val.u16 = xSize * 1000;
    payload[10] = (char)val.u8[1];
    payload[11] = (char)val.u8[0];
    val.u16 = ySize * 1000;
    payload[14] = (char)val.u8[1];
    payload[15] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVERotation(quint8 keyer, float rotation)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[3] = (char)0x10;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    val.u16 = rotation * 10;
    payload[26] = (char)val.u8[1];
    payload[27] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVELightSource(quint8 keyer, float direction, quint8 altitude)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[1] = (char)0x0c;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    val.u16 = direction * 10;
    payload[48] = (char)val.u8[1];
    payload[49] = (char)val.u8[0];
    payload[50] = (char)altitude;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEDropShadowEnabled(quint8 keyer, bool enabled)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[3] = (char)0x40;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[29] = (char)enabled;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderEnabled(quint8 keyer, bool enabled)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[3] = (char)0x20;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[28] = (char)enabled;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderStyle(quint8 keyer, quint8 style)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[3] = (char)0x80;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[30] = (char)style;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderColorH(quint8 keyer, float h)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[2] = (char)0x80;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    val.u16 = h * 10;
    payload[42] = (char)val.u8[1];
    payload[43] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderColorS(quint8 keyer, float s)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[1] = (char)0x01;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    val.u16 = s * 10;
    payload[44] = (char)val.u8[1];
    payload[45] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderColorL(quint8 keyer, float l)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[1] = (char)0x02;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    val.u16 = l * 10;
    payload[46] = (char)val.u8[1];
    payload[47] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderColor(quint8 keyer, const QColor& color)
{
    setUpstreamKeyDVEBorderColorH(keyer, qMax((qreal)0.0, color.hslHueF()) * 360.0);
    setUpstreamKeyDVEBorderColorS(keyer, color.hslSaturationF() * 100);
    setUpstreamKeyDVEBorderColorL(keyer, color.lightnessF() * 100);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderWidth(quint8 keyer, float outside, float inside)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[2] = (char)0x03;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    val.u16 = outside * 100;
    payload[32] = (char)val.u8[1];
    payload[33] = (char)val.u8[0];
    val.u16 = inside * 100;
    payload[34] = (char)val.u8[1];
    payload[35] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderSoften(quint8 keyer, quint8 outside, quint8 inside)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[2] = (char)0x0c;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[36] = (char)outside;
    payload[37] = (char)inside;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderOpacity(quint8 keyer, quint8 opacity)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[2] = (char)0x40;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[40] = (char)opacity;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderBevelPosition(quint8 keyer, float position)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[2] = (char)0x20;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[39] = (char)(position * 100);

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEBorderBevelSoften(quint8 keyer, float soften)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[2] = (char)0x10;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[38] = (char)(soften * 100);

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVERate(quint8 keyer, quint8 rate)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[0] = (char)0x02;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[60] = (char)rate;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEKeyFrame(quint8 keyer, quint8 keyFrame)
{
    QByteArray cmd("SFKF");
    QByteArray payload(4, (char)0x0);

    payload[0] = (char)m_id;
    payload[1] = (char)keyer;
    payload[2] = (char)keyFrame;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::runUpstreamKeyTo(quint8 keyer, quint8 position, quint8 direction)
{
    QByteArray cmd("RFlK");
    QByteArray payload(8, (char)0x0);

    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[4] = (char)position;

    if(position == 4)
    {
        payload[0] = (char)0x02; // This is needed else the direction will be ignore
        payload[5] = (char)direction;
    }

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyFlyEnabled(quint8 keyer, bool enable)
{
    QByteArray cmd("CKTp");
    QByteArray payload(8, (char)0x0);

    payload[0] = (char)0x02;
    payload[1] = (char)m_id;
    payload[2] = (char)keyer;
    payload[4] = (char)enable;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEMaskEnabled(quint8 keyer, bool enable)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);

    payload[1] = (char)0x10;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;
    payload[51] = (char)enable;

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::setUpstreamKeyDVEMask(quint8 keyer, float top, float bottom, float left, float right)
{
    QByteArray cmd("CKDV");
    QByteArray payload(64, (char)0x0);
    QAtem::U16_U8 val;

    payload[0] = (char)0x01;
    payload[1] = (char)0xe0;
    payload[4] = (char)m_id;
    payload[5] = (char)keyer;

    val.u16 = top * 1000;
    payload[52] = (char)val.u8[1];
    payload[53] = (char)val.u8[0];
    val.u16 = bottom * 1000;
    payload[54] = (char)val.u8[1];
    payload[55] = (char)val.u8[0];
    val.u16 = left * 1000;
    payload[56] = (char)val.u8[1];
    payload[57] = (char)val.u8[0];
    val.u16 = right * 1000;
    payload[58] = (char)val.u8[1];
    payload[59] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemMixEffect::onPrgI(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if (me == m_id)
    {
        quint16 old = m_programInput;
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(8);
        val.u8[0] = (quint8)payload.at(9);
        m_programInput = val.u16;
        emit programInputChanged(m_id, old, m_programInput);
    }
}

void QAtemMixEffect::onPrvI(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if (me == m_id)
    {
        quint16 old = m_previewInput;
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(8);
        val.u8[0] = (quint8)payload.at(9);
        m_previewInput = val.u16;
        emit previewInputChanged(m_id, old, m_previewInput);
    }
}

void QAtemMixEffect::onTrPr(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        m_transitionPreviewEnabled = (payload.at(7) > 0);

        emit transitionPreviewChanged(m_id, m_transitionPreviewEnabled);
    }
}

void QAtemMixEffect::onTrPs(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        m_transitionFrameCount = (quint8)payload.at(8);
        m_transitionPosition = ((quint8)payload.at(11) | ((quint8)payload.at(10) << 8));

        emit transitionFrameCountChanged(m_id, m_transitionFrameCount);
        emit transitionPositionChanged(m_id, m_transitionPosition);
    }
}

void QAtemMixEffect::onTrSS(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if (me == m_id)
    {
        m_currentTransitionStyle = (quint8)payload.at(7); // Bit 0 = Mix, 1 = Dip, 2 = Wipe, 3 = DVE and 4 = Stinger, only bit 0-2 available on TVS
        m_keyersOnCurrentTransition = ((quint8)payload.at(8) & 0x1f); // Bit 0 = Background, 1-4 = keys, only bit 0 and 1 available on TVS
        m_nextTransitionStyle = (quint8)payload.at(9); // Bit 0 = Mix, 1 = Dip, 2 = Wipe, 3 = DVE and 4 = Stinger, only bit 0-2 available on TVS
        m_keyersOnNextTransition = ((quint8)payload.at(10) & 0x1f); // Bit 0 = Background, 1-4 = keys, only bit 0 and 1 available on TVS

        emit nextTransitionStyleChanged(m_id, m_nextTransitionStyle);
        emit keyersOnNextTransitionChanged(m_id, m_keyersOnNextTransition);
        emit currentTransitionStyleChanged(m_id, m_currentTransitionStyle);
        emit keyersOnCurrentTransitionChanged(m_id, m_keyersOnCurrentTransition);
    }
}

void QAtemMixEffect::onFtbS(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        m_fadeToBlackEnabled = (bool)payload.at(7);
        m_fadeToBlackFading = (bool)payload.at(8);
        m_fadeToBlackFrameCount = (quint8)payload.at(9);

        emit fadeToBlackChanged(m_id, m_fadeToBlackFading, m_fadeToBlackEnabled);
        emit fadeToBlackFrameCountChanged(m_id, m_fadeToBlackFrameCount);
    }
}

void QAtemMixEffect::onFtbP(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        m_fadeToBlackFrames = (quint8)payload.at(7);

        emit fadeToBlackFramesChanged(m_id, m_fadeToBlackFrames);
    }
}

void QAtemMixEffect::onTMxP(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        m_mixFrames = (quint8)payload.at(7);

        emit mixFramesChanged(m_id, m_mixFrames);
    }
}

void QAtemMixEffect::onTDpP(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        QAtem::U16_U8 val;
        m_dipFrames = (quint8)payload.at(7);
        val.u8[1] = (quint8)payload.at(8);
        val.u8[0] = (quint8)payload.at(9);
        m_dipSource = val.u16;

        emit dipFramesChanged(m_id, m_dipFrames);
        emit dipSourceChanged(m_id, m_dipSource);
    }
}

void QAtemMixEffect::onTWpP(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        m_wipeFrames = (quint8)payload.at(7);
        m_wipeType = (quint8)payload.at(8);

        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(10);
        val.u8[0] = (quint8)payload.at(11);
        m_wipeBorderWidth = val.u16;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        m_wipeBorderSource = val.u16;
        val.u8[1] = (quint8)payload.at(14);
        val.u8[0] = (quint8)payload.at(15);
        m_wipeSymmetry = val.u16;
        val.u8[1] = (quint8)payload.at(16);
        val.u8[0] = (quint8)payload.at(17);
        m_wipeBorderSoftness = val.u16;
        val.u8[1] = (quint8)payload.at(18);
        val.u8[0] = (quint8)payload.at(19);
        m_wipeXPosition = val.u16;
        val.u8[1] = (quint8)payload.at(20);
        val.u8[0] = (quint8)payload.at(21);
        m_wipeYPosition = val.u16;
        m_wipeReverseDirection = (quint8)payload.at(22);
        m_wipeFlipFlop = (quint8)payload.at(23);

        emit wipeFramesChanged(m_id, m_wipeFrames);
        emit wipeBorderWidthChanged(m_id, m_wipeBorderWidth);
        emit wipeBorderSourceChanged(m_id, m_wipeBorderSource);
        emit wipeBorderSoftnessChanged(m_id, m_wipeBorderSoftness);
        emit wipeTypeChanged(m_id, m_wipeType);
        emit wipeSymmetryChanged(m_id, m_wipeSymmetry);
        emit wipeXPositionChanged(m_id, m_wipeXPosition);
        emit wipeYPositionChanged(m_id, m_wipeYPosition);
        emit wipeReverseDirectionChanged(m_id, m_wipeReverseDirection);
        emit wipeFlipFlopChanged(m_id, m_wipeFlipFlop);
    }
}

void QAtemMixEffect::onTDvP(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        m_dveRate = (quint8)payload.at(7);
        m_dveEffect = (quint8)payload.at(9);
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(10);
        val.u8[0] = (quint8)payload.at(11);
        m_dveFillSource = val.u16;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        m_dveKeySource = val.u16;
        m_dveKeyEnabled = (bool)payload.at(14);
        m_dvePreMultipliedKeyEnabled = (bool)payload.at(15);
        val.u8[1] = (quint8)payload.at(16);
        val.u8[0] = (quint8)payload.at(17);
        m_dveKeyClip = val.u16 / 10.0;
        val.u8[1] = (quint8)payload.at(18);
        val.u8[0] = (quint8)payload.at(19);
        m_dveKeyGain= val.u16 / 10.0;
        m_dveEnableInvertKey = (bool)payload.at(20);
        m_dveReverseDirection = (bool)payload.at(21);
        m_dveFlipFlopDirection = (bool)payload.at(22);

        emit dveRateChanged(m_id, m_dveRate);
        emit dveEffectChanged(m_id, m_dveEffect);
        emit dveFillSourceChanged(m_id, m_dveFillSource);
        emit dveKeySourceChanged(m_id, m_dveKeySource);
        emit dveEnableKeyChanged(m_id, m_dveKeyEnabled);
        emit dveEnablePreMultipliedKeyChanged(m_id, m_dvePreMultipliedKeyEnabled);
        emit dveKeyClipChanged(m_id, m_dveKeyClip);
        emit dveKeyGainChanged(m_id, m_dveKeyGain);
        emit dveEnableInvertKeyChanged(m_id, m_dveEnableInvertKey);
        emit dveReverseDirectionChanged(m_id, m_dveReverseDirection);
        emit dveFlipFlopDirectionChanged(m_id, m_dveFlipFlopDirection);
    }
}

void QAtemMixEffect::onTStP(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        m_stingerSource = (quint8)payload.at(7);
        m_stingerPreMultipliedKeyEnabled = (quint8)payload.at(8);
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(10);
        val.u8[0] = (quint8)payload.at(11);
        m_stingerClip = val.u16 / 10.0;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        m_stingerGain = val.u16 / 10.0;
        m_stingerInvertKeyEnabled = (bool)payload.at(14);
        val.u8[1] = (quint8)payload.at(16);
        val.u8[0] = (quint8)payload.at(17);
        m_stingerPreRoll = val.u16;
        val.u8[1] = (quint8)payload.at(18);
        val.u8[0] = (quint8)payload.at(19);
        m_stingerClipDuration = val.u16;
        val.u8[1] = (quint8)payload.at(20);
        val.u8[0] = (quint8)payload.at(21);
        m_stingerTriggerPoint = val.u16;
        val.u8[1] = (quint8)payload.at(22);
        val.u8[0] = (quint8)payload.at(23);
        m_stingerMixRate = val.u16;

        emit stingerSourceChanged(m_id, m_stingerSource);
        emit stingerEnablePreMultipliedKeyChanged(m_id, m_stingerPreMultipliedKeyEnabled);
        emit stingerClipChanged(m_id, m_stingerClip);
        emit stingerGainChanged(m_id, m_stingerGain);
        emit stingerEnableInvertKeyChanged(m_id, m_stingerInvertKeyEnabled);
        emit stingerPreRollChanged(m_id, m_stingerPreRoll);
        emit stingerClipDurationChanged(m_id, m_stingerClipDuration);
        emit stingerTriggerPointChanged(m_id, m_stingerTriggerPoint);
        emit stingerMixRateChanged(m_id, m_stingerMixRate);
    }
}

void QAtemMixEffect::onKeOn(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        quint8 index = (quint8)payload.at(7);
        if (index >= m_upstreamKeys.count()) return;
        m_upstreamKeys[index]->m_onAir = (quint8)payload.at(8);

        emit upstreamKeyOnAirChanged(m_id, index, m_upstreamKeys[index]->m_onAir);
    }
}

void QAtemMixEffect::onKeBP(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        QAtem::U16_U8 val;
        quint8 index = (quint8)payload.at(7);
        if (index >= m_upstreamKeys.count()) return;
        m_upstreamKeys[index]->m_type = (quint8)payload.at(8);
        m_upstreamKeys[index]->m_enableFly = (bool)payload.at(11);
        val.u8[1] = (quint8)payload[12];
        val.u8[0] = (quint8)payload[13];
        m_upstreamKeys[index]->m_fillSource = val.u16;
        val.u8[1] = (quint8)payload[14];
        val.u8[0] = (quint8)payload[15];
        m_upstreamKeys[index]->m_keySource = val.u16;
        m_upstreamKeys[index]->m_enableMask = (quint8)payload.at(16);
        val.u8[1] = (quint8)payload.at(18);
        val.u8[0] = (quint8)payload.at(19);
        m_upstreamKeys[index]->m_topMask = (qint16)val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(20);
        val.u8[0] = (quint8)payload.at(21);
        m_upstreamKeys[index]->m_bottomMask = (qint16)val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(22);
        val.u8[0] = (quint8)payload.at(23);
        m_upstreamKeys[index]->m_leftMask = (qint16)val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(24);
        val.u8[0] = (quint8)payload.at(25);
        m_upstreamKeys[index]->m_rightMask = (qint16)val.u16 / 1000.0;

        emit upstreamKeyTypeChanged(m_id, index, m_upstreamKeys[index]->m_type);
        emit upstreamKeyEnableFlyChanged(m_id, index, m_upstreamKeys[index]->m_enableFly);
        emit upstreamKeyFillSourceChanged(m_id, index, m_upstreamKeys[index]->m_fillSource);
        emit upstreamKeyKeySourceChanged(m_id, index, m_upstreamKeys[index]->m_keySource);
        emit upstreamKeyEnableMaskChanged(m_id, index, m_upstreamKeys[index]->m_enableMask);
        emit upstreamKeyTopMaskChanged(m_id, index, m_upstreamKeys[index]->m_topMask);
        emit upstreamKeyBottomMaskChanged(m_id, index, m_upstreamKeys[index]->m_bottomMask);
        emit upstreamKeyLeftMaskChanged(m_id, index, m_upstreamKeys[index]->m_leftMask);
        emit upstreamKeyRightMaskChanged(m_id, index, m_upstreamKeys[index]->m_rightMask);
    }
}

void QAtemMixEffect::onKeLm(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        quint8 index = (quint8)payload.at(7);
        if (index >= m_upstreamKeys.count()) return;
        m_upstreamKeys[index]->m_lumaPreMultipliedKey = (quint8)payload.at(8);
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(10);
        val.u8[0] = (quint8)payload.at(11);
        m_upstreamKeys[index]->m_lumaClip = val.u16 / 10.0;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        m_upstreamKeys[index]->m_lumaGain = val.u16 / 10.0;
        m_upstreamKeys[index]->m_lumaInvertKey = (quint8)payload.at(14);

        emit upstreamKeyLumaPreMultipliedKeyChanged(m_id, index, m_upstreamKeys[index]->m_lumaPreMultipliedKey);
        emit upstreamKeyLumaClipChanged(m_id, index, m_upstreamKeys[index]->m_lumaClip);
        emit upstreamKeyLumaGainChanged(m_id, index, m_upstreamKeys[index]->m_lumaGain);
        emit upstreamKeyLumaInvertKeyChanged(m_id, index, m_upstreamKeys[index]->m_lumaInvertKey);
    }
}

void QAtemMixEffect::onKeCk(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        quint8 index = (quint8)payload.at(7);
        if (index >= m_upstreamKeys.count()) return;
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(8);
        val.u8[0] = (quint8)payload.at(9);
        m_upstreamKeys[index]->m_chromaHue = val.u16 / 10.0;
        val.u8[1] = (quint8)payload.at(10);
        val.u8[0] = (quint8)payload.at(11);
        m_upstreamKeys[index]->m_chromaGain = val.u16 / 10.0;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        m_upstreamKeys[index]->m_chromaYSuppress = val.u16 / 10.0;
        val.u8[1] = (quint8)payload.at(14);
        val.u8[0] = (quint8)payload.at(15);
        m_upstreamKeys[index]->m_chromaLift = val.u16 / 10.0;
        m_upstreamKeys[index]->m_chromaNarrowRange = (quint8)payload.at(16);

        emit upstreamKeyChromaHueChanged(m_id, index, m_upstreamKeys[index]->m_chromaHue);
        emit upstreamKeyChromaGainChanged(m_id, index, m_upstreamKeys[index]->m_chromaGain);
        emit upstreamKeyChromaYSuppressChanged(m_id, index, m_upstreamKeys[index]->m_chromaYSuppress);
        emit upstreamKeyChromaLiftChanged(m_id, index, m_upstreamKeys[index]->m_chromaLift);
        emit upstreamKeyChromaNarrowRangeChanged(m_id, index, m_upstreamKeys[index]->m_chromaNarrowRange);
    }
}

void QAtemMixEffect::onKePt(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        quint8 index = (quint8)payload.at(7);
        if (index >= m_upstreamKeys.count()) return;
        m_upstreamKeys[index]->m_patternPattern = (quint8)payload.at(8);
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(10);
        val.u8[0] = (quint8)payload.at(11);
        m_upstreamKeys[index]->m_patternSize = val.u16 / 100.0;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        m_upstreamKeys[index]->m_patternSymmetry = val.u16 / 100.0;
        val.u8[1] = (quint8)payload.at(14);
        val.u8[0] = (quint8)payload.at(15);
        m_upstreamKeys[index]->m_patternSoftness = val.u16 / 100.0;
        val.u8[1] = (quint8)payload.at(16);
        val.u8[0] = (quint8)payload.at(17);
        m_upstreamKeys[index]->m_patternXPosition = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(18);
        val.u8[0] = (quint8)payload.at(19);
        m_upstreamKeys[index]->m_patternYPosition = val.u16 / 1000.0;
        m_upstreamKeys[index]->m_patternInvertPattern = (quint8)payload.at(20);

        emit upstreamKeyPatternPatternChanged(m_id, index, m_upstreamKeys[index]->m_patternPattern);
        emit upstreamKeyPatternSizeChanged(m_id, index, m_upstreamKeys[index]->m_patternSize);
        emit upstreamKeyPatternSymmetryChanged(m_id, index, m_upstreamKeys[index]->m_patternSymmetry);
        emit upstreamKeyPatternSoftnessChanged(m_id, index, m_upstreamKeys[index]->m_patternSoftness);
        emit upstreamKeyPatternXPositionChanged(m_id, index, m_upstreamKeys[index]->m_patternXPosition);
        emit upstreamKeyPatternYPositionChanged(m_id, index, m_upstreamKeys[index]->m_patternYPosition);
        emit upstreamKeyPatternInvertPatternChanged(m_id, index, m_upstreamKeys[index]->m_patternInvertPattern);
    }
}

void QAtemMixEffect::onKeDV(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        quint8 index = (quint8)payload.at(7);
        if (index >= m_upstreamKeys.count()) return;
        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        m_upstreamKeys[index]->m_dveXSize = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(16);
        val.u8[0] = (quint8)payload.at(17);
        m_upstreamKeys[index]->m_dveYSize = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(20);
        val.u8[0] = (quint8)payload.at(21);
        m_upstreamKeys[index]->m_dveXPosition = (qint16)val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(24);
        val.u8[0] = (quint8)payload.at(25);
        m_upstreamKeys[index]->m_dveYPosition = (qint16)val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(28);
        val.u8[0] = (quint8)payload.at(29);
        m_upstreamKeys[index]->m_dveRotation = (qint16)val.u16 / 10.0;
        m_upstreamKeys[index]->m_dveEnableBorder = (bool)payload.at(30);
        m_upstreamKeys[index]->m_dveEnableDropShadow = (bool)payload.at(31);
        m_upstreamKeys[index]->m_dveBorderStyle = (quint8)payload.at(32);
        val.u8[1] = (quint8)payload.at(34);
        val.u8[0] = (quint8)payload.at(35);
        m_upstreamKeys[index]->m_dveBorderOutsideWidth = val.u16 / 100.0;
        val.u8[1] = (quint8)payload.at(36);
        val.u8[0] = (quint8)payload.at(37);
        m_upstreamKeys[index]->m_dveBorderInsideWidth = val.u16 / 100.0;
        m_upstreamKeys[index]->m_dveBorderOutsideSoften = (quint8)payload.at(38);
        m_upstreamKeys[index]->m_dveBorderInsideSoften = (quint8)payload.at(39);
        m_upstreamKeys[index]->m_dveBorderBevelSoften = (quint8)payload.at(40) / 100.0;
        m_upstreamKeys[index]->m_dveBorderBevelPosition = ((quint8)payload.at(41)) / 100.0;
        m_upstreamKeys[index]->m_dveBorderOpacity = (quint8)payload.at(42);
        QAtem::U16_U8 h, s, l;

        h.u8[1] = (quint8)payload.at(44);
        h.u8[0] = (quint8)payload.at(45);
        s.u8[1] = (quint8)payload.at(46);
        s.u8[0] = (quint8)payload.at(47);
        l.u8[1] = (quint8)payload.at(48);
        l.u8[0] = (quint8)payload.at(49);

        QColor color;
        float hf = ((h.u16 / 10) % 360) / 360.0;
        color.setHslF(hf, s.u16 / 1000.0, l.u16 / 1000.0);
        m_upstreamKeys[index]->m_dveBorderColor = color;
        val.u8[1] = (quint8)payload.at(50);
        val.u8[0] = (quint8)payload.at(51);
        m_upstreamKeys[index]->m_dveLightSourceDirection = val.u16 / 10.0;
        m_upstreamKeys[index]->m_dveLightSourceAltitude = (quint8)payload.at(52);
        m_upstreamKeys[index]->m_dveMaskEnabled = (bool)payload.at(53);
        val.u8[1] = (quint8)payload.at(54);
        val.u8[0] = (quint8)payload.at(55);
        m_upstreamKeys[index]->m_dveMaskTop = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(56);
        val.u8[0] = (quint8)payload.at(57);
        m_upstreamKeys[index]->m_dveMaskBottom = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(58);
        val.u8[0] = (quint8)payload.at(59);
        m_upstreamKeys[index]->m_dveMaskLeft = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(60);
        val.u8[0] = (quint8)payload.at(61);
        m_upstreamKeys[index]->m_dveMaskRight = val.u16 / 1000.0;
        m_upstreamKeys[index]->m_dveRate = (quint8)payload.at(62);

        emit upstreamKeyDVEXPositionChanged(m_id, index, m_upstreamKeys[index]->m_dveXPosition);
        emit upstreamKeyDVEYPositionChanged(m_id, index, m_upstreamKeys[index]->m_dveYPosition);
        emit upstreamKeyDVEXSizeChanged(m_id, index, m_upstreamKeys[index]->m_dveXSize);
        emit upstreamKeyDVEYSizeChanged(m_id, index, m_upstreamKeys[index]->m_dveYSize);
        emit upstreamKeyDVERotationChanged(m_id, index, m_upstreamKeys[index]->m_dveRotation);
        emit upstreamKeyDVEEnableDropShadowChanged(m_id, index, m_upstreamKeys[index]->m_dveEnableDropShadow);
        emit upstreamKeyDVELighSourceDirectionChanged(m_id, index, m_upstreamKeys[index]->m_dveLightSourceDirection);
        emit upstreamKeyDVELightSourceAltitudeChanged(m_id, index, m_upstreamKeys[index]->m_dveLightSourceAltitude);
        emit upstreamKeyDVEEnableBorderChanged(m_id, index, m_upstreamKeys[index]->m_dveEnableBorder);
        emit upstreamKeyDVEBorderStyleChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderStyle);
        emit upstreamKeyDVEBorderColorChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderColor);
        emit upstreamKeyDVEBorderOutsideWidthChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderOutsideWidth);
        emit upstreamKeyDVEBorderInsideWidthChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderInsideWidth);
        emit upstreamKeyDVEBorderOutsideSoftenChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderOutsideSoften);
        emit upstreamKeyDVEBorderInsideSoftenChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderInsideSoften);
        emit upstreamKeyDVEBorderOpacityChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderOpacity);
        emit upstreamKeyDVEBorderBevelPositionChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderBevelPosition);
        emit upstreamKeyDVEBorderBevelSoftenChanged(m_id, index, m_upstreamKeys[index]->m_dveBorderBevelSoften);
        emit upstreamKeyDVERateChanged(m_id, index, m_upstreamKeys[index]->m_dveRate);
        emit upstreamKeyDVEMaskEnabledChanged(m_id, index, m_upstreamKeys[index]->m_dveMaskEnabled);
        emit upstreamKeyDVEMaskTopChanged(m_id, index, m_upstreamKeys[index]->m_dveMaskTop);
        emit upstreamKeyDVEMaskBottomChanged(m_id, index, m_upstreamKeys[index]->m_dveMaskBottom);
        emit upstreamKeyDVEMaskLeftChanged(m_id, index, m_upstreamKeys[index]->m_dveMaskLeft);
        emit upstreamKeyDVEMaskRightChanged(m_id, index, m_upstreamKeys[index]->m_dveMaskRight);
    }
}

void QAtemMixEffect::onKeFS(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        quint8 index = (quint8)payload.at(7);
        if (index >= m_upstreamKeys.count()) return;
        m_upstreamKeys[index]->m_dveKeyFrameASet = (bool)payload.at(8);
        m_upstreamKeys[index]->m_dveKeyFrameBSet = (bool)payload.at(9);

        emit upstreamKeyDVEKeyFrameASetChanged(m_id, index, m_upstreamKeys[index]->m_dveKeyFrameASet);
        emit upstreamKeyDVEKeyFrameBSetChanged(m_id, index, m_upstreamKeys[index]->m_dveKeyFrameBSet);
    }
}

void QAtemMixEffect::onKKFP(const QByteArray& payload)
{
    quint8 me = (quint8)payload.at(6);

    if(me == m_id)
    {
        quint8 index = (quint8)payload.at(7);
        if (index >= m_upstreamKeys.count()) return;
        quint8 frameIndex = (quint8)payload.at(8);
        QAtem::DveKeyFrame keyFrame;

        QAtem::U16_U8 val;
        val.u8[1] = (quint8)payload.at(12);
        val.u8[0] = (quint8)payload.at(13);
        keyFrame.size.setWidth(val.u16 / 1000.0);
        val.u8[1] = (quint8)payload.at(16);
        val.u8[0] = (quint8)payload.at(17);
        keyFrame.size.setHeight(val.u16 / 1000.0);
        val.u8[1] = (quint8)payload.at(20);
        val.u8[0] = (quint8)payload.at(21);
        keyFrame.position.setX(val.u16 / 1000.0);
        val.u8[1] = (quint8)payload.at(24);
        val.u8[0] = (quint8)payload.at(25);
        keyFrame.position.setY(val.u16 / 1000.0);
        val.u8[1] = (quint8)payload.at(28);
        val.u8[0] = (quint8)payload.at(29);
        keyFrame.rotation = val.u16 / 10.0;
        val.u8[1] = (quint8)payload.at(30);
        val.u8[0] = (quint8)payload.at(31);
        keyFrame.borderOutsideWidth = val.u16 / 100.0;
        val.u8[1] = (quint8)payload.at(32);
        val.u8[0] = (quint8)payload.at(33);
        keyFrame.borderInsideWidth = val.u16 / 100.0;
        keyFrame.borderOutsideSoften = (quint8)payload.at(34);
        keyFrame.borderInsideSoften = (quint8)payload.at(35);
        keyFrame.borderBevelSoften = (quint8)payload.at(36);
        keyFrame.borderBevelPosition = ((quint8)payload.at(37)) / 100.0;
        keyFrame.borderOpacity = (quint8)payload.at(38);
        QAtem::U16_U8 h, s, l;

        h.u8[1] = (quint8)payload.at(40);
        h.u8[0] = (quint8)payload.at(41);
        s.u8[1] = (quint8)payload.at(42);
        s.u8[0] = (quint8)payload.at(43);
        l.u8[1] = (quint8)payload.at(44);
        l.u8[0] = (quint8)payload.at(45);

        QColor color;
        float hf = ((h.u16 / 10) % 360) / 360.0;
        color.setHslF(hf, s.u16 / 1000.0, l.u16 / 1000.0);
        keyFrame.borderColor = color;
        val.u8[1] = (quint8)payload.at(46);
        val.u8[0] = (quint8)payload.at(47);
        keyFrame.lightSourceDirection = val.u16 / 10.0;
        keyFrame.lightSourceAltitude = (quint8)payload.at(48);
        val.u8[1] = (quint8)payload.at(50);
        val.u8[0] = (quint8)payload.at(51);
        keyFrame.maskTop = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(52);
        val.u8[0] = (quint8)payload.at(53);
        keyFrame.maskBottom = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(54);
        val.u8[0] = (quint8)payload.at(55);
        keyFrame.maskLeft = val.u16 / 1000.0;
        val.u8[1] = (quint8)payload.at(56);
        val.u8[0] = (quint8)payload.at(57);
        keyFrame.maskRight = val.u16 / 1000.0;

        m_upstreamKeys[index]->m_keyFrames[frameIndex - 1] = keyFrame;

        emit upstreamKeyDVEKeyFrameChanged(m_id, index, frameIndex);
    }
}
