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

#include "qatemcameracontrol.h"
#include "qatemconnection.h"

QAtemCameraControl::QAtemCameraControl(QAtemConnection *parent) :
    QObject(parent), m_atemConnection(parent)
{
    m_atemConnection->registerCommand("CCdP", this, "onCCdP");
}

QAtemCameraControl::~QAtemCameraControl()
{
    m_atemConnection->unregisterCommand("CCdP", this);

    qDeleteAll(m_cameras);
}

void QAtemCameraControl::onCCdP(const QByteArray& payload)
{
    quint8 input = (quint8)payload.at(6);
    quint8 adjustmentDomain = (quint8)payload.at(7);
    quint8 feature = (quint8)payload.at(8);

    if(input == 0)
    {
        qDebug() << "[onCCdP] Unhandled input:" << input;
        return;
    }

    if(input >= m_cameras.count())
    {
        m_cameras.append(new QAtem::Camera(input));
    }

    QAtem::Camera *camera = m_cameras[input - 1];
    QAtem::U16_U8 val;

    switch(adjustmentDomain)
    {
    case 0: //Lens
        switch(feature)
        {
        case 0: //Focus
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->focus = val.u16;
            camera->autoFocused = false;
            break;
        case 1: //Auto focused
            camera->autoFocused = true;
            break;
        case 3: //Iris
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->iris = val.u16;
            break;
        case 9: //Zoom
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->zoomSpeed = (qint16)val.u16;
            break;
        default:
            qDebug() << "[doCCdP] Unknown lens feature:" << feature;
            break;
        }

        break;
    case 1: //Camera
        switch(feature)
        {
        case 1: //Gain
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->gain = val.u16;
            break;
        case 2: //White balance
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->whiteBalance = val.u16;
            break;
        case 5: //Shutter
            val.u8[1] = (quint8)payload.at(24);
            val.u8[0] = (quint8)payload.at(25);
            camera->shutter = val.u16;
            break;
        default:
            qDebug() << "[doCCdP] Unknown camera feature:" << feature;
            break;
        }

        break;
    case 8: //Chip
        switch(feature)
        {
        case 0: //Lift
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->liftR = (qint16)val.u16 / 4096.0;
            val.u8[1] = (quint8)payload.at(24);
            val.u8[0] = (quint8)payload.at(25);
            camera->liftB = (qint16)val.u16 / 4096.0;
            val.u8[1] = (quint8)payload.at(26);
            val.u8[0] = (quint8)payload.at(27);
            camera->liftG = (qint16)val.u16 / 4096.0;
            val.u8[1] = (quint8)payload.at(28);
            val.u8[0] = (quint8)payload.at(29);
            camera->liftY = (qint16)val.u16 / 4096.0;
            break;
        case 1: //Gamma
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->gammaR = (qint16)val.u16 / 8192.0;
            val.u8[1] = (quint8)payload.at(24);
            val.u8[0] = (quint8)payload.at(25);
            camera->gammaB = (qint16)val.u16 / 8192.0;
            val.u8[1] = (quint8)payload.at(26);
            val.u8[0] = (quint8)payload.at(27);
            camera->gammaG = (qint16)val.u16 / 8192.0;
            val.u8[1] = (quint8)payload.at(28);
            val.u8[0] = (quint8)payload.at(29);
            camera->gammaY = (qint16)val.u16 / 8192.0;
            break;
        case 2: //Gain
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->gainR = (qint16)val.u16 / 2048.0;
            val.u8[1] = (quint8)payload.at(24);
            val.u8[0] = (quint8)payload.at(25);
            camera->gainB = (qint16)val.u16 / 2048.0;
            val.u8[1] = (quint8)payload.at(26);
            val.u8[0] = (quint8)payload.at(27);
            camera->gainG = (qint16)val.u16 / 2048.0;
            val.u8[1] = (quint8)payload.at(28);
            val.u8[0] = (quint8)payload.at(29);
            camera->gainY = (qint16)val.u16 / 2048.0;
            break;
        case 3: //Aperture
//            qDebug() << payload.mid(6).toHex();
            break;
        case 4: //Contrast
            val.u8[1] = (quint8)payload.at(24);
            val.u8[0] = (quint8)payload.at(25);
            camera->contrast = qRound(((qint16)val.u16 / 4096.0) * 100);
            break;
        case 5: //Lum
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->lumMix = qRound(((qint16)val.u16 / 2048.0) * 100);
            break;
        case 6: //Hue & Saturation
            val.u8[1] = (quint8)payload.at(22);
            val.u8[0] = (quint8)payload.at(23);
            camera->hue = 180 + qRound(((qint16)val.u16 / 2048.0) * 180);
            val.u8[1] = (quint8)payload.at(24);
            val.u8[0] = (quint8)payload.at(25);
            camera->saturation = qRound(((qint16)val.u16 / 4096.0) * 100);
            break;
        default:
            qDebug() << "[doCCdP] Unknown chip feature:" << feature;
            break;
        }

        break;
    default:
        qDebug() << "[onCCdP] Unknown adjustment domain:" << adjustmentDomain;
    }

    emit cameraChanged(input);
}

void QAtemCameraControl::setFocus(quint8 input, quint16 focus)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x00; //Lens
    payload[2] = (char)0x00; //Focus
    payload[3] = (char)0x00; //0: Set focus, 1: Add focus to previous value
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x01; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = focus;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::activeAutoFocus(quint8 input)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x00; //Lens
    payload[2] = (char)0x01; //Auto focus

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setIris(quint8 input, quint16 iris)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x00; //Lens
    payload[2] = (char)0x03; //Iris
    payload[3] = (char)0x00; //0: Set iris, 1: Add iris to previous value
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x01; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = iris;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setZoomSpeed(quint8 input, qint16 zoom)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x00; //Lens
    payload[2] = (char)0x09; //Zoom
    payload[3] = (char)0x00; //0: Set focus, 1: Add zoom to previous value
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x01; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = zoom;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setGain(quint8 input, quint16 gain)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x01; //Camera
    payload[2] = (char)0x01; //Gain
    payload[4] = (char)0x01; //Unknown, but needs to be set
    payload[7] = (char)0x01; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = gain;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setWhiteBalance(quint8 input, quint16 wb)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x01; //Camera
    payload[2] = (char)0x02; //White balance
    payload[4] = (char)0x02; //Unknown, but needs to be set
    payload[9] = (char)0x01; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = wb;
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setShutter(quint8 input, quint16 shutter)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x01; //Camera
    payload[2] = (char)0x05; //Shutter
    payload[4] = (char)0x03; //Unknown, but needs to be set
    payload[11] = (char)0x01; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = shutter;
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setLift(quint8 input, float r, float g, float b, float y)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x08; //Chip
    payload[2] = (char)0x00; //Lift
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x04; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = qRound(r * 4096);
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];
    val.u16 = qRound(g * 4096);
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];
    val.u16 = qRound(b * 4096);
    payload[20] = (char)val.u8[1];
    payload[21] = (char)val.u8[0];
    val.u16 = qRound(y * 4096);
    payload[22] = (char)val.u8[1];
    payload[23] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setGamma(quint8 input, float r, float g, float b, float y)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x08; //Chip
    payload[2] = (char)0x01; //Gamma
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x04; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = qRound(r * 8192);
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];
    val.u16 = qRound(g * 8192);
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];
    val.u16 = qRound(b * 8192);
    payload[20] = (char)val.u8[1];
    payload[21] = (char)val.u8[0];
    val.u16 = qRound(y * 8192);
    payload[22] = (char)val.u8[1];
    payload[23] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setGain(quint8 input, float r, float g, float b, float y)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x08; //Chip
    payload[2] = (char)0x02; //Gain
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x04; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = qRound(r * 2048);
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];
    val.u16 = qRound(g * 2048);
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];
    val.u16 = qRound(b * 2048);
    payload[20] = (char)val.u8[1];
    payload[21] = (char)val.u8[0];
    val.u16 = qRound(y * 2048);
    payload[22] = (char)val.u8[1];
    payload[23] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setContrast(quint8 input, quint8 contrast)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x08; //Chip
    payload[2] = (char)0x04; //Contrast
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x02; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = qRound((contrast / 100.0) * 4096.0);
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setLumMix(quint8 input, quint8 mix)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x08; //Chip
    payload[2] = (char)0x05; //Lum
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x01; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = qRound((mix / 100.0) * 2048.0);
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setHueSaturation(quint8 input, quint16 hue, quint8 saturation)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, (char)0x0);

    payload[0] = (char)input;
    payload[1] = (char)0x08; //Chip
    payload[2] = (char)0x06; //Hue & Saturation
    payload[4] = (char)0x80; //Unknown, but needs to be set
    payload[9] = (char)0x02; //Unknown, but needs to be set
    QAtem::U16_U8 val;
    val.u16 = qRound(((hue - 180) / 180.0) * 2048.0);
    payload[16] = (char)val.u8[1];
    payload[17] = (char)val.u8[0];
    val.u16 = qRound((saturation / 100.0) * 4096.0);
    payload[18] = (char)val.u8[1];
    payload[19] = (char)val.u8[0];

    m_atemConnection->sendCommand(cmd, payload);
}
