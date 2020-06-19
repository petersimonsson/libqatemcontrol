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

#define QATEM_LENS 0x00
#define QATEM_CAMERA 0x01
#define QATEM_CHIP 0x08

#define QATEM_LENS_FOCUS 0x00
#define QATEM_LENS_AUTOFOCUS 0x01
#define QATEM_LENS_IRIS 0x03
#define QATEM_LENS_ZOOM 0x09

#define QATEM_CAMERA_GAIN 0x01
#define QATEM_CAMERA_WHITE_BALANCE 0x02
#define QATEM_CAMERA_SHUTTER 0x05

#define QATEM_CHIP_LIFT 0x00
#define QATEM_CHIP_GAMMA 0x01
#define QATEM_CHIP_GAIN 0x02
#define QATEM_CHIP_CONTRAST 0x04
#define QATEM_CHIP_LUM_MIX 0x05
#define QATEM_HUE_SATURATION 0x06

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
    quint8 input = static_cast<quint8>(payload.at(6));
    quint8 adjustmentDomain = static_cast<quint8>(payload.at(7));
    quint8 feature = static_cast<quint8>(payload.at(8));

    if(input == 0)
    {
        qDebug() << "[onCCdP] Unhandled input:" << input;
        return;
    }

    if(input >= m_cameras.count())
    {
        for(quint8 index = static_cast<quint8>(m_cameras.count()); index < input; index++)
        {
            m_cameras.append(new QAtem::Camera(index));
        }
    }

    QAtem::Camera *camera = m_cameras[input - 1];
    QAtem::U16_U8 val;

    switch(adjustmentDomain)
    {
    case 0: //Lens
        switch(feature)
        {
        case 0: //Focus
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->focus = val.u16;
            camera->autoFocused = false;
            break;
        case 1: //Auto focused
            camera->autoFocused = true;
            break;
        case 3: //Iris
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->iris = val.u16;
            break;
        case 9: //Zoom
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->zoomSpeed = static_cast<qint16>(val.u16);
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
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->gain = val.u16;
            break;
        case 2: //White balance
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->whiteBalance = val.u16;
            break;
        case 5: //Shutter
            val.u8[1] = static_cast<quint8>(payload.at(24));
            val.u8[0] = static_cast<quint8>(payload.at(25));
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
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->liftR = static_cast<qint16>(val.u16) / 4096.0f;
            val.u8[1] = static_cast<quint8>(payload.at(24));
            val.u8[0] = static_cast<quint8>(payload.at(25));
            camera->liftB = static_cast<qint16>(val.u16) / 4096.0f;
            val.u8[1] = static_cast<quint8>(payload.at(26));
            val.u8[0] = static_cast<quint8>(payload.at(27));
            camera->liftG = static_cast<qint16>(val.u16) / 4096.0f;
            val.u8[1] = static_cast<quint8>(payload.at(28));
            val.u8[0] = static_cast<quint8>(payload.at(29));
            camera->liftY = static_cast<qint16>(val.u16) / 4096.0f;
            break;
        case 1: //Gamma
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->gammaR = static_cast<qint16>(val.u16) / 8192.0f;
            val.u8[1] = static_cast<quint8>(payload.at(24));
            val.u8[0] = static_cast<quint8>(payload.at(25));
            camera->gammaB = static_cast<qint16>(val.u16) / 8192.0f;
            val.u8[1] = static_cast<quint8>(payload.at(26));
            val.u8[0] = static_cast<quint8>(payload.at(27));
            camera->gammaG = static_cast<qint16>(val.u16) / 8192.0f;
            val.u8[1] = static_cast<quint8>(payload.at(28));
            val.u8[0] = static_cast<quint8>(payload.at(29));
            camera->gammaY = static_cast<qint16>(val.u16) / 8192.0f;
            break;
        case 2: //Gain
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->gainR = static_cast<qint16>(val.u16) / 2048.0f;
            val.u8[1] = static_cast<quint8>(payload.at(24));
            val.u8[0] = static_cast<quint8>(payload.at(25));
            camera->gainB = static_cast<qint16>(val.u16) / 2048.0f;
            val.u8[1] = static_cast<quint8>(payload.at(26));
            val.u8[0] = static_cast<quint8>(payload.at(27));
            camera->gainG = static_cast<qint16>(val.u16) / 2048.0f;
            val.u8[1] = static_cast<quint8>(payload.at(28));
            val.u8[0] = static_cast<quint8>(payload.at(29));
            camera->gainY = static_cast<qint16>(val.u16) / 2048.0f;
            break;
        case 3: //Aperture
//            qDebug() << payload.mid(6).toHex();
            break;
        case 4: //Contrast
            val.u8[1] = static_cast<quint8>(payload.at(24));
            val.u8[0] = static_cast<quint8>(payload.at(25));
            camera->contrast = static_cast<quint8>(qRound((static_cast<qint16>(val.u16) / 4096.0) * 100));
            break;
        case 5: //Lum
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->lumMix = static_cast<quint8>(qRound((static_cast<qint16>(val.u16) / 2048.0) * 100));
            break;
        case 6: //Hue & Saturation
            val.u8[1] = static_cast<quint8>(payload.at(22));
            val.u8[0] = static_cast<quint8>(payload.at(23));
            camera->hue = static_cast<quint8>(180 + qRound((static_cast<qint16>(val.u16) / 2048.0) * 180));
            val.u8[1] = static_cast<quint8>(payload.at(24));
            val.u8[0] = static_cast<quint8>(payload.at(25));
            camera->saturation = static_cast<quint8>(qRound((static_cast<qint16>(val.u16) / 4096.0) * 100));
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

void QAtemCameraControl::sendCCmd(quint8 input, quint8 hardware, quint8 command, bool append, quint8 valueType, QList<QAtem::U16_U8> values)
{
    if(input < 1)
    {
        return;
    }

    QByteArray cmd("CCmd");
    QByteArray payload(24, 0x0);

    payload[0] = static_cast<char>(input);
    payload[1] = static_cast<char>(hardware);
    payload[2] = static_cast<char>(command);
    payload[3] = append;
    payload[4] = static_cast<char>(valueType);


    int index = 0;

    switch (valueType)
    {
    case 0x01:
        payload[7] = static_cast<char>(values.count());
        foreach(const QAtem::U16_U8 &val, values)
        {
            payload[16 + 2 * index] = static_cast<char>(val.u8[1]);
            payload[17 + 2 * index] = static_cast<char>(val.u8[0]);
            index++;
        }
        break;
    case 0x03:
        payload[11] = static_cast<char>(values.count());
        foreach(const QAtem::U16_U8 &val, values)
        {
            payload[18 + 2 * index] = static_cast<char>(val.u8[1]);
            payload[19 + 2 * index] = static_cast<char>(val.u8[0]);
            index++;
        }
        break;
    case 0x02:
        /* fallthrough */
    case 0x80:
        payload[9] = static_cast<char>(values.count());
        foreach(const QAtem::U16_U8 &val, values)
        {
            payload[16 + 2 * index] = static_cast<char>(val.u8[1]);
            payload[17 + 2 * index] = static_cast<char>(val.u8[0]);
            index++;
        }
        break;
    }

    m_atemConnection->sendCommand(cmd, payload);
}

void QAtemCameraControl::setFocus(quint8 input, quint16 focus)
{
    QAtem::U16_U8 val;
    val.u16 = focus;

    sendCCmd(input, QATEM_LENS, QATEM_LENS_FOCUS, false, 0x80, QList<QAtem::U16_U8>() << val);
}

void QAtemCameraControl::activeAutoFocus(quint8 input)
{
    sendCCmd(input, QATEM_LENS, QATEM_LENS_AUTOFOCUS, false, 0x0, QList<QAtem::U16_U8>());
}

void QAtemCameraControl::setIris(quint8 input, quint16 iris)
{
    QAtem::U16_U8 val;
    val.u16 = iris;

    sendCCmd(input, QATEM_LENS, QATEM_LENS_IRIS, false, 0x80, QList<QAtem::U16_U8>() << val);
}

void QAtemCameraControl::setZoomSpeed(quint8 input, qint16 zoom)
{
    QAtem::U16_U8 val;
    val.u16 = static_cast<quint16>(zoom);

    sendCCmd(input, QATEM_LENS, QATEM_LENS_ZOOM, false, 0x80, QList<QAtem::U16_U8>() << val);
}

void QAtemCameraControl::setGain(quint8 input, quint16 gain)
{
    QAtem::U16_U8 val;
    val.u16 = gain;

    sendCCmd(input, QATEM_CAMERA, QATEM_CAMERA_GAIN, false, 0x01, QList<QAtem::U16_U8>() << val);
}

void QAtemCameraControl::setWhiteBalance(quint8 input, quint16 wb)
{
    QAtem::U16_U8 val;
    val.u16 = wb;

    sendCCmd(input, QATEM_CAMERA, QATEM_CAMERA_WHITE_BALANCE, false, 0x02, QList<QAtem::U16_U8>() << val);
}

void QAtemCameraControl::setShutter(quint8 input, quint16 shutter)
{
    QAtem::U16_U8 val;
    val.u16 = shutter;

    sendCCmd(input, QATEM_CAMERA, QATEM_CAMERA_SHUTTER, false, 0x03, QList<QAtem::U16_U8>() << val);
}

void QAtemCameraControl::setLift(quint8 input, float r, float g, float b, float y)
{
    QList<QAtem::U16_U8> values;
    QAtem::U16_U8 val;
    val.u16 = static_cast<quint16>(qRound(r * 4096));
    values << val;
    val.u16 = static_cast<quint16>(qRound(g * 4096));
    values << val;
    val.u16 = static_cast<quint16>(qRound(b * 4096));
    values << val;
    val.u16 = static_cast<quint16>(qRound(y * 4096));
    values << val;

    sendCCmd(input, QATEM_CHIP, QATEM_CHIP_LIFT, false, 0x80, values);
}

void QAtemCameraControl::setGamma(quint8 input, float r, float g, float b, float y)
{
    QList<QAtem::U16_U8> values;
    QAtem::U16_U8 val;
    val.u16 = static_cast<quint16>(qRound(r * 8192));
    values << val;
    val.u16 = static_cast<quint16>(qRound(g * 8192));
    values << val;
    val.u16 = static_cast<quint16>(qRound(b * 8192));
    values << val;
    val.u16 = static_cast<quint16>(qRound(y * 8192));
    values << val;

    sendCCmd(input, QATEM_CHIP, QATEM_CHIP_GAMMA, false, 0x80, values);
}

void QAtemCameraControl::setGain(quint8 input, float r, float g, float b, float y)
{
    QList<QAtem::U16_U8> values;
    QAtem::U16_U8 val;
    val.u16 = static_cast<quint16>(qRound(r * 2048));
    values << val;
    val.u16 = static_cast<quint16>(qRound(g * 2048));
    values << val;
    val.u16 = static_cast<quint16>(qRound(b * 2048));
    values << val;
    val.u16 = static_cast<quint16>(qRound(y * 2048));
    values << val;

    sendCCmd(input, QATEM_CHIP, QATEM_CHIP_GAIN, false, 0x80, values);
}

void QAtemCameraControl::setContrast(quint8 input, quint8 contrast)
{
    QList<QAtem::U16_U8> values;
    QAtem::U16_U8 val;
    val.u16 = 0x00;
    values << val;
    val.u16 = static_cast<quint16>(qRound((contrast / 100.0) * 4096.0));
    values << val;

    sendCCmd(input, QATEM_CHIP, QATEM_CHIP_CONTRAST, false, 0x80, values);
}

void QAtemCameraControl::setLumMix(quint8 input, quint8 mix)
{
    QAtem::U16_U8 val;
    val.u16 = static_cast<quint16>(qRound((mix / 100.0) * 2048.0));

    sendCCmd(input, QATEM_CHIP, QATEM_CHIP_LUM_MIX, false, 0x80, QList<QAtem::U16_U8>() << val);
}

void QAtemCameraControl::setHueSaturation(quint8 input, quint16 hue, quint8 saturation)
{
    QList<QAtem::U16_U8> values;
    QAtem::U16_U8 val;
    val.u16 = static_cast<quint16>(qRound(((hue - 180) / 180.0) * 2048.0));
    values << val;
    val.u16 = static_cast<quint16>(qRound((saturation / 100.0) * 4096.0));
    values << val;

    sendCCmd(input, QATEM_CHIP, QATEM_HUE_SATURATION, false, 0x80, values);
}
