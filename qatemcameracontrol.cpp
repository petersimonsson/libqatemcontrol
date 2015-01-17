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
