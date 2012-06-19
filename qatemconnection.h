/*
Copyright 2012  Peter Simonsson <peter.simonsson@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of
the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef QATEMCONNECTION_H
#define QATEMCONNECTION_H

#include <QObject>
#include <QUdpSocket>
#include <QColor>

typedef union
{
    quint16 u16;
    quint8 u8[2];
} U16_U8;

typedef union
{
    quint32 u32;
    quint8 u8[4];
} U32_U8;

class QAtemConnection : public QObject
{
    Q_OBJECT
public:
    enum Command
    {
        Cmd_NoCommand = 0x0,
        Cmd_AckRequest = 0x1,
        Cmd_HelloPacket = 0x2,
        Cmd_Resend = 0x4,
        Cmd_Undefined = 0x8,
        Cmd_Ack = 0x10
    };

    Q_DECLARE_FLAGS(Commands, Command)

    struct CommandHeader
    {
        quint8 bitmask;
        quint16 size;
        quint16 uid;
        quint16 ackId;
        quint16 packageId;

        CommandHeader()
        {
            bitmask = size = uid = ackId = packageId = 0;
        }
    };

    struct InputInfo
    {
        quint8 index;
        QString longText;
        QString shortText;
    };

    struct MediaInfo
    {
        quint8 index;
        bool used;
        QString name;
    };

    explicit QAtemConnection(QObject* parent = NULL);

    /// Connect to ATEM switcher at @p address
    void connectToSwitcher(const QHostAddress& address);

    /// @returns the index of the input that is on program
    quint8 programInput() const { return m_programInput; }
    /// @returns the index of the input that is on preview
    quint8 previewInput() const { return m_previewInput; }

    /// @returns the tally state of the input @p id
    quint8 tallyState(quint8 id) const;

    bool transitionPreviewEnabled() const { return m_transitionPreviewEnabled; }
    quint8 transitionFrameCount() const { return m_transitionFrameCount; }
    quint16 transitionPosition() const { return m_transitionPosition; }
    quint8 keyersOnNextTransition() const { return m_keyersOnNextTransition; }
    quint8 transitionStyle() const { return m_transitionStyle; }

    /// @returns true if fade to black is on.
    bool fadeToBlackEnabled() const { return m_fadeToBlackEnabled; }
    /// @returns number of frames left of fade to black transition.
    quint8 fadeToBlackFrameCount() const { return m_fadeToBlackFrameCount; }
    /// @returns duration in number of frames for the fade to black transition.
    quint8 fadeToBlackFrames() const { return m_fadeToBlackFrames; }

    /// @returns true if downstream key @p keyer is on air
    bool downstreamKeyOn(quint8 keyer) const;
    /// @returns true if downstream key @p keyer is tied to next transition
    bool downstreamKeyTie(quint8 keyer) const;
    /// @returns number of frames left of key transition for @p keyer
    quint8 downstreamKeyFrameCount(quint8 keyer) const { return m_downstreamKeyFrameCount.value(keyer); }
    /// @returns duration in number of frames for key transition of dsk @p keyer
    quint8 downstreamKeyFrames(quint8 keyer) const { return m_downstreamKeyFrames.value(keyer); }
    /// @returns the input selected as fill source for downstream key @p keyer
    quint8 downstreamKeyFillSource(quint8 keyer) const { return m_downstreamKeyFillSource.value(keyer); }
    /// @returns the input selected as key source for downstream key @p keyer
    quint8 downstreamKeyKeySource(quint8 keyer) const { return m_downstreamKeyKeySource.value(keyer); }

    /// @returns true if upstream key @p keyer is on air
    bool upstreamKeyOn(quint8 keyer) const;

    QColor colorGeneratorColor(quint8 generator) const;

    quint8 mediaPlayerType(quint8 player) const;
    quint8 mediaPlayerSelectedStill(quint8 player) const;
    quint8 mediaPlayerSelectedClip(quint8 player) const;

    quint8 auxSource(quint8 aux) const;

    QString productInformation() const { return m_productInformation; }
    quint16 majorVersion() const { return m_majorversion; }
    quint16 minorVersion() const { return m_minorversion; }

    InputInfo inputInfo(quint8 index) const { return m_inputInfos.value(index); }

    MediaInfo mediaInfo(quint8 index) const { return m_mediaInfos.value(index); }

    /// @returns index of the multi view layout, 0 = prg/prv on top, 1 = prg/prv on bottom, 2 = prg/prv on left, 3 = prg/prv on right
    quint8 multiViewLayoutIndex() const { return m_multiViewLayoutIndex; }
    /// @returns index of the input mapped to @p multiViewOutput
    quint8 multiViewInput(quint8 multiViewOutput) const { return m_multiViewInputs.value(multiViewOutput); }

    /// @returns index of the video format in use. 0 = 525i5994, 1 = 625i50, 2 = 720p50, 3 = 720p5994, 4 = 1080i50, 5 = 1080i5994
    quint8 videoFormatIndex() const { return m_videoFormatIndex; }

    /// @returns duration in number of frames for mix transition
    quint8 mixFrames() const { return m_mixFrames; }
    /// @returns duration in number of frames for dip transition
    quint8 dipFrames() const { return m_dipFrames; }
    /// @returns duration in number of frames for wipe transition
    quint8 wipeFrames() const { return m_wipeFrames; }
    /// @returns duration in number of frames for dve transition
    quint8 dveFrames() const { return m_dveFrames; }
    /// @returns duration in number of frames for sting transition
    quint8 stingFrames() const { return m_stingFrames; }

public slots:
    void changeProgramInput(char index);
    void changePreviewInput(char index);

    void doCut();
    void doAuto();
    void toggleFadeToBlack();
    void setTransitionPosition(quint16 position);
    void signalTransitionPositionChangeDone();
    void setTransitionPreview(bool state);
    void setTransitionType(quint8 type);

    void setUpstreamKeyOn(quint8 keyer, bool state);
    void setUpstreamKeyOnNextTransition(quint8 keyer, bool state);
    void setBackgroundOnNextTransition(bool state);

    void setDownstreamKeyOn(quint8 keyer, bool state);
    void setDownstreamKeyTie(quint8 keyer, bool state);
    void doDownstreamKeyAuto(quint8 keyer);
    void setDownstreamKeyFillSource(quint8 keyer, quint8 source);
    void setDownstreamKeyKeySource(quint8 keyer, quint8 source);

    void saveSettings();
    void clearSettings();

    void setColorGeneratorColor(quint8 generator, const QColor& color);

    void setMediaPlayerSource(quint8 player, bool clip, quint8 source);

protected slots:
    void handleSocketData();

    void handleError(QAbstractSocket::SocketError);

protected:
    QByteArray createCommandHeader(Commands bitmask, quint16 payloadSize, quint16 uid, quint16 ackId, quint16 undefined1, quint16 undefined2);

    QAtemConnection::CommandHeader parseCommandHeader(const QByteArray& datagram) const;
    void parsePayLoad(const QByteArray& datagram);

    void sendDatagram(const QByteArray& datagram);
    void sendCommand(const QByteArray& cmd, const QByteArray &payload);

private:
    QUdpSocket* m_socket;

    QHostAddress m_address;
    quint16 m_port;

    quint16 m_packetCounter;
    bool m_isInitialized;
    quint16 m_currentUid;

    quint8 m_programInput;
    quint8 m_previewInput;
    quint8 m_tallyStateCount;
    QHash<quint8, quint8> m_tallyStates;

    bool m_transitionPreviewEnabled;
    quint8 m_transitionFrameCount;
    quint16 m_transitionPosition;
    quint8 m_keyersOnNextTransition;
    quint8 m_transitionStyle;

    bool m_fadeToBlackEnabled;
    quint8 m_fadeToBlackFrameCount;
    quint8 m_fadeToBlackFrames;

    QHash<quint8, bool> m_downstreamKeyOn;
    QHash<quint8, bool> m_downstreamKeyTie;
    QHash<quint8, quint8> m_downstreamKeyFrames;
    QHash<quint8, quint8> m_downstreamKeyFrameCount;
    QHash<quint8, quint8> m_downstreamKeyFillSource;
    QHash<quint8, quint8> m_downstreamKeyKeySource;

    QHash<quint8, bool> m_upstreamKeyOn;

    QHash<quint8, QColor> m_colorGeneratorColors;

    QHash<quint8, quint8> m_mediaPlayerType;
    QHash<quint8, quint8> m_mediaPlayerSelectedStill;
    QHash<quint8, quint8> m_mediaPlayerSelectedClip;

    QHash<quint8, quint8> m_auxSource;

    QString m_productInformation;
    quint16 m_majorversion;
    quint16 m_minorversion;

    QHash<quint8, InputInfo> m_inputInfos;

    QHash<quint8, MediaInfo> m_mediaInfos;

    QHash<quint8, quint8> m_multiViewInputs;
    quint8 m_multiViewLayoutIndex;

    quint8 m_videoFormatIndex;

    quint8 m_mixFrames;
    quint8 m_dipFrames;
    quint8 m_wipeFrames;
    quint8 m_dveFrames;
    quint8 m_stingFrames;

signals:
    void connected();
    void socketError(const QString& errorString);

    void programInputChanged(quint8 id);
    void previewInputChanged(quint8 id);

    void tallyStatesChanged();

    void transitionPreviewChanged(bool state);
    void transitionFrameCountChanged(quint8 count);
    void transitionPositionChanged(quint16 count);
    void transitionStyleChanged(quint8 style);
    void keyersOnNextTransitionChanged(quint8 keyers);

    void fadeToBlackChanged(bool enabled);
    void fadeToBlackFrameCountChanged(quint8 count);
    void fadeToBlackFramesChanged(quint8 frames);

    void downstreamKeyOnChanged(quint8 keyer, bool state);
    void downstreamKeyTieChanged(quint8 keyer, bool state);
    void downstreamKeyFrameCountChanged(quint8 keyer, quint8 count);
    void downstreamKeyFramesChanged(quint8 keyer, quint8 frames);
    void downstreamKeySourcesChanged(quint8 keyer, quint8 fill, quint8 key);

    void upstreamKeyOnChanged(quint8 keyer, bool state);

    void colorGeneratorColorChanged(quint8 generator, const QColor& color);

    void mediaPlayerChanged(quint8 player, quint8 type, quint8 still, quint8 clip);

    void auxSourceChanged(quint8 aux, quint8 source);

    void inputInfoChanged(const QAtemConnection::InputInfo& info);

    void mediaInfoChanged(const QAtemConnection::MediaInfo& info);

    void productInformationChanged(const QString& info);
    void versionChanged(quint16 major, quint16 minor);

    void timeChanged(quint32 time);

    void mixFramesChanged(quint8 frames);
    void dipFramesChanged(quint8 frames);
    void wipeFramesChanged(quint8 frames);
    void dveFramesChanged(quint8 frames);
    void stingFramesChanged(quint8 frames);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAtemConnection::Commands)

#endif //QATEMCONNECTION_H
