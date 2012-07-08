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
#include "qdownstreamkeysettings.h"

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
        quint8 type;
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

    /// @returns true if transition preview is enabled
    bool transitionPreviewEnabled() const { return m_transitionPreviewEnabled; }
    /// @returns number of frames left of transition
    quint8 transitionFrameCount() const { return m_transitionFrameCount; }
    /// @returns percent left of transition
    quint16 transitionPosition() const { return m_transitionPosition; }
    quint8 keyersOnNextTransition() const { return m_keyersOnNextTransition; }
    /// @returns index of selected transition style
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
    quint8 downstreamKeyFrameCount(quint8 keyer) const { return m_downstreamKey.value(keyer).m_frameCount; }
    /// @returns duration in number of frames for key transition of dsk @p keyer
    quint8 downstreamKeyFrames(quint8 keyer) const { return m_downstreamKey.value(keyer).m_frames; }
    /// @returns the input selected as fill source for downstream key @p keyer
    quint8 downstreamKeyFillSource(quint8 keyer) const { return m_downstreamKey.value(keyer).m_fillSource; }
    /// @returns the input selected as key source for downstream key @p keyer
    quint8 downstreamKeyKeySource(quint8 keyer) const { return m_downstreamKey.value(keyer).m_keySource; }
    /// @returns true if the should be inverted for downstream key @p keyer
    bool downstreamKeyInvertKey(quint8 keyer) const { return m_downstreamKey.value(keyer).m_keySource; }
    /// @returns true if the key is pre multiplied for downstream key @p keyer
    bool donwstreamKeyPreMultiplied(quint8 keyer) const { return m_downstreamKey.value(keyer).m_preMultiplied; }
    /// @returns the clip set for downstream key @p keyer
    quint16 downstreamKeyClip(quint8 keyer) const { return m_downstreamKey.value(keyer).m_clip; }
    /// @returns the gain set for downstream key @p keyer
    quint16 downstreamKeyGain(quint8 keyer) const { return m_downstreamKey.value(keyer).m_gain; }
    /// @returns true if the mask for downstream key @p keyer is enabled
    bool downstreamKeyEnableMask(quint8 keyer) const { return m_downstreamKey.value(keyer).m_enableMask; }
    /// @returns the top position of the mask for downstream key @p keyer
    qint16 downstreamKeyTopMask(quint8 keyer) const { return m_downstreamKey.value(keyer).m_topMask; }
    /// @returns the bottom position of the mask for downstream key @p keyer
    qint16 downstreamKeyBottomMask(quint8 keyer) const { return m_downstreamKey.value(keyer).m_bottomMask; }
    /// @returns the left position of the mask for downstream key @p keyer
    qint16 downstreamKeyLeftMask(quint8 keyer) const { return m_downstreamKey.value(keyer).m_leftMask; }
    /// @returns the right position of the mask for downstream key @p keyer
    qint16 downstreamKeyRightMask(quint8 keyer) const { return m_downstreamKey.value(keyer).m_rightMask; }

    /// @returns true if upstream key @p keyer is on air
    bool upstreamKeyOn(quint8 keyer) const;
    /// @returns the key type for upstream key @p keyer, 0 = luma, 1 = chroma, 2 = pattern, 3 = DVE
    quint8 upstreamKeyType(quint8 keyer) const { return m_upstreamKeyType.value(keyer); }
    /// @returns the source used as fill for upstream key @p keyer
    quint8 upstreamKeyFillSource(quint8 keyer) const { return m_upstreamKeyFillSource.value(keyer); }
    /// @returns the source used as key for upstream key @p keyer
    quint8 upstreamKeyKeySource(quint8 keyer) const { return m_upstreamKeyKeySource.value(keyer); }
    /// @returns true if the mask is enabled for upstream key @p keyer
    bool upstreamKeyEnableMask(quint8 keyer) const { return m_upstreamKeyEnableMask.value(keyer); }
    /// @returns top mask for upstream key @p keyer
    qint16 upstreamKeyTopMask(quint8 keyer) const { return m_upstreamKeyTopMask.value(keyer); }
    /// @returns bottom mask for upstream key @p keyer
    qint16 upstreamKeyBottomMask(quint8 keyer) const { return m_upstreamKeyBottomMask.value(keyer); }
    /// @returns left mask for upstream key @p keyer
    qint16 upstreamKeyLeftMask(quint8 keyer) const { return m_upstreamKeyLeftMask.value(keyer); }
    /// @returns right mask for upstream key @p keyer
    qint16 upstreamKeyRightMask(quint8 keyer) const { return m_upstreamKeyRightMask.value(keyer); }
    /// @returns true if the key is pre multiplied for luma upstream key @p keyer
    bool upstreamKeyLumaPreMultipliedKey(quint8 keyer) const { return m_upstreamKeyLumaPreMultipliedKey.value(keyer); }
    /// @returns true if the key source should be inverted for luma upstream key @p keyer
    bool upstreamKeyLumaInvertKey(quint8 keyer) const { return m_upstreamKeyLumaInvertKey.value(keyer); }
    /// @returns clip for luma upstream key @p keyer
    quint16 upstreamKeyLumaClip(quint8 keyer) const { return m_upstreamKeyLumaClip.value(keyer); }
    /// @returns gain for luma upstream key @p keyer
    quint16 upstreamKeyLumaGain(quint8 keyer) const { return m_upstreamKeyLumaGain.value(keyer); }
    /// @returns hue for chroma upstream key @p keyer
    quint16 upstreamKeyChromaHue(quint8 keyer) const { return m_upstreamKeyChromaHue.value(keyer); }
    /// @returns gain for chroma upstream key @p keyer
    quint16 upstreamKeyChromaGain(quint8 keyer) const { return m_upstreamKeyChromaGain.value(keyer); }
    /// @returns y suppress for chroma upstream key @p keyer
    quint16 upstreamKeyChromaYSuppress(quint8 keyer) const { return m_upstreamKeyChromaYSuppress.value(keyer); }
    /// @returns lift for chroma upstream key @p keyer
    quint16 upstreamKeyChromaLift(quint8 keyer) const { return m_upstreamKeyChromaLift.value(keyer); }
    /// @returns true if chroma upstream key @p keyer should have narrow chroma key range
    bool upstreamKeyChromaNarrowRange(quint8 keyer) const { return m_upstreamKeyChromaNarrowRange.value(keyer); }
    /// @returns pattern of pattern upstream key @p keyer
    quint8 upstreamKeyPatternPattern(quint8 keyer) const { return m_upstreamKeyPatternPattern.value(keyer); }
    /// @returns true if pattern upstream key @p keyer should invert the pattern
    bool upstreamKeyPatternInvertPattern(quint8 keyer) const { return m_upstreamKeyPatternInvertPattern.value(keyer); }
    /// @returns size for pattern upstream key @p keyer
    quint16 upstreamKeyPatternSize(quint8 keyer) const { return m_upstreamKeyPatternSize.value(keyer); }
    /// @returns symmetry for pattern upstream key @p keyer
    quint16 upstreamKeyPatternSymmetry(quint8 keyer) const { return m_upstreamKeyPatternSymmetry.value(keyer); }
    /// @returns softness for pattern upstream key @p keyer
    quint16 upstreamKeyPatternSoftness(quint8 keyer) const { return m_upstreamKeyPatternSoftness.value(keyer); }
    /// @returns x position for pattern upstream key @p keyer
    quint16 upstreamKeyPatternXPosition(quint8 keyer) const { return m_upstreamKeyPatternXPosition.value(keyer); }
    /// @returns y position for pattern upstream key @p keyer
    quint16 upstreamKeyPatternYPosition(quint8 keyer) const { return m_upstreamKeyPatternYPosition.value(keyer); }

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
    quint8 multiViewLayout() const { return m_multiViewLayout; }
    /// @returns index of the input mapped to @p multiViewOutput
    quint8 multiViewInput(quint8 multiViewOutput) const { return m_multiViewInputs.value(multiViewOutput); }

    /// @returns index of the video format in use. 0 = 525i5994, 1 = 625i50, 2 = 720p50, 3 = 720p5994, 4 = 1080i50, 5 = 1080i5994
    quint8 videoFormat() const { return m_videoFormat; }

    /// @returns duration in number of frames for mix transition
    quint8 mixFrames() const { return m_mixFrames; }

    /// @returns duration in number of frames for dip transition
    quint8 dipFrames() const { return m_dipFrames; }

    /// @returns duration in number of frames for wipe transition
    quint8 wipeFrames() const { return m_wipeFrames; }
    /// @returns border width for wipe transition
    quint16 wipeBorderWidth() const { return m_wipeBorderWidth; }
    /// @returns border softness for wipe transition
    quint16 wipeBorderSoftness() const { return m_wipeBorderSoftness; }
    /// @returns type of wipe transition
    quint8 wipeType() const { return m_wipeType; }
    /// @returns symmetry of wipe transition
    quint16 wipeSymmetry() const { return m_wipeSymmetry; }
    /// @returns x position of wipe transition
    quint16 wipeXPosition() const { return m_wipeXPosition; }
    /// @returns y position of wipe transition
    quint16 wipeYPosition() const { return m_wipeYPosition; }
    /// @returns true if wipe transition direction should be reversed
    bool wipeReverseDirection() const { return m_wipeReverseDirection; }
    /// @returns true if wipe transition direction should flip flop
    bool wipeFlipFlop() const { return m_wipeFlipFlop; }

    /// @returns duration in number of frames for dve transition
    quint8 dveFrames() const { return m_dveFrames; }

    /// @returns duration in number of frames for sting transition
    quint8 stingFrames() const { return m_stingFrames; }

    /// @returns the border source index, used for both dip and wipe transition
    quint8 borderSource() const { return m_borderSource; }

public slots:
    void changeProgramInput(char index);
    void changePreviewInput(char index);

    void doCut();
    void doAuto();
    void toggleFadeToBlack();
    void setFadeToBlackFrameRate(quint8 frames);
    void setTransitionPosition(quint16 position);
    void signalTransitionPositionChangeDone();
    void setTransitionPreview(bool state);
    void setTransitionType(quint8 type);

    void setUpstreamKeyOn(quint8 keyer, bool state);
    void setUpstreamKeyOnNextTransition(quint8 keyer, bool state);
    void setBackgroundOnNextTransition(bool state);
    void setUpstreamKeyType(quint8 keyer, quint8 type);
    void setUpstreamKeyFillSource(quint8 keyer, quint8 source);
    void setUpstreamKeyKeySource(quint8 keyer, quint8 source);
    void setUpstreamKeyEnableMask(quint8 keyer, bool enable);
    void setUpstreamKeyTopMask(quint8 keyer, qint16 value);
    void setUpstreamKeyBottomMask(quint8 keyer, qint16 value);
    void setUpstreamKeyLeftMask(quint8 keyer, qint16 value);
    void setUpstreamKeyRightMask(quint8 keyer, qint16 value);
    void setUpstreamKeyLumaPreMultipliedKey(quint8 keyer, bool preMultiplied);
    void setUpstreamKeyLumaInvertKey(quint8 keyer, bool invert);
    void setUpstreamKeyLumaClip(quint8 keyer, quint16 clip);
    void setUpstreamKeyLumaGain(quint8 keyer, quint16 gain);
    void setUpstreamKeyChromaHue(quint8 keyer, quint16 hue);
    void setUpstreamKeyChromaGain(quint8 keyer, quint16 hue);
    void setUpstreamKeyChromaYSuppress(quint8 keyer, quint16 ySuppress);
    void setUpstreamKeyChromaLift(quint8 keyer, quint16 lift);
    void setUpstreamKeyChromaNarrowRange(quint8 keyer, bool narrowRange);
    void setUpstreamKeyPatternPattern(quint8 keyer, quint8 pattern);
    void setUpstreamKeyPatternInvertPattern(quint8 keyer, bool invert);
    void setUpstreamKeyPatternSize(quint8 keyer, quint16 size);
    void setUpstreamKeyPatternSymmetry(quint8 keyer, quint16 symmetry);
    void setUpstreamKeyPatternSoftness(quint8 keyer, quint16 softness);
    void setUpstreamKeyPatternXPosition(quint8 keyer, quint16 xPosition);
    void setUpstreamKeyPatternYPosition(quint8 keyer, quint16 yPosition);

    void setDownstreamKeyOn(quint8 keyer, bool state);
    void setDownstreamKeyTie(quint8 keyer, bool state);
    void doDownstreamKeyAuto(quint8 keyer);
    void setDownstreamKeyFillSource(quint8 keyer, quint8 source);
    void setDownstreamKeyKeySource(quint8 keyer, quint8 source);
    void setDownstreamKeyFrameRate(quint8 keyer, quint8 frames);
    void setDownstreamKeyInvertKey(quint8 keyer, bool invert);
    void setDownstreamKeyPreMultiplied(quint8 keyer, bool preMultiplied);
    void setDownstreamKeyClip(quint8 keyer, float clip);
    void setDownstreamKeyGain(quint8 keyer, float gain);
    void setDownstreamKeyEnableMask(quint8 keyer, bool enable);
    void setDownstreamKeyTopMask(quint8 keyer, float value);
    void setDownstreamKeyBottomMask(quint8 keyer, float value);
    void setDownstreamKeyLeftMask(quint8 keyer, float value);
    void setDownstreamKeyRightMask(quint8 keyer, float value);

    void saveSettings();
    void clearSettings();

    void setColorGeneratorColor(quint8 generator, const QColor& color);

    void setMediaPlayerSource(quint8 player, bool clip, quint8 source);

    void setMixFrames(quint8 frames);

    void setDipFrames(quint8 frames);

    void setBorderSource(quint8 index);

    void setWipeFrames(quint8 frames);
    void setWipeBorderWidth(quint16 width);
    void setWipeBorderSoftness(quint16 softness);
    void setWipeType(quint8 type);
    void setWipeSymmetry(quint16 value);
    void setWipeXPosition(quint16 value);
    void setWipeYPosition(quint16 value);
    void setWipeReverseDirection(bool reverse);
    void setWipeFlipFlop(bool flipFlop);

    void setAuxSource(quint8 aux, quint8 source);

    void setInputType(quint8 input, quint8 type);
    void setInputLongName(quint8 input, const QString& name);
    void setInputShortName(quint8 input, const QString& name);

    void setVideoFormat(quint8 format);

    void setMultiViewLayout(quint8 layout);

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

    QHash<quint8, QDownstreamKeySettings> m_downstreamKey;

    QHash<quint8, bool> m_upstreamKeyOn;
    QHash<quint8, quint8> m_upstreamKeyType;
    QHash<quint8, quint8> m_upstreamKeyFillSource;
    QHash<quint8, quint8> m_upstreamKeyKeySource;
    QHash<quint8, bool> m_upstreamKeyEnableMask;
    QHash<quint8, qint16> m_upstreamKeyTopMask;
    QHash<quint8, qint16> m_upstreamKeyBottomMask;
    QHash<quint8, qint16> m_upstreamKeyLeftMask;
    QHash<quint8, qint16> m_upstreamKeyRightMask;
    QHash<quint8, bool> m_upstreamKeyLumaPreMultipliedKey;
    QHash<quint8, bool> m_upstreamKeyLumaInvertKey;
    QHash<quint8, quint16> m_upstreamKeyLumaClip;
    QHash<quint8, quint16> m_upstreamKeyLumaGain;
    QHash<quint8, quint16> m_upstreamKeyChromaHue;
    QHash<quint8, quint16> m_upstreamKeyChromaGain;
    QHash<quint8, quint16> m_upstreamKeyChromaYSuppress;
    QHash<quint8, quint16> m_upstreamKeyChromaLift;
    QHash<quint8, bool> m_upstreamKeyChromaNarrowRange;
    QHash<quint8, quint8> m_upstreamKeyPatternPattern;
    QHash<quint8, bool> m_upstreamKeyPatternInvertPattern;
    QHash<quint8, quint16> m_upstreamKeyPatternSize;
    QHash<quint8, quint16> m_upstreamKeyPatternSymmetry;
    QHash<quint8, quint16> m_upstreamKeyPatternSoftness;
    QHash<quint8, quint16> m_upstreamKeyPatternXPosition;
    QHash<quint8, quint16> m_upstreamKeyPatternYPosition;

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
    quint8 m_multiViewLayout;

    quint8 m_videoFormat;

    quint8 m_mixFrames;

    quint8 m_dipFrames;

    quint8 m_wipeFrames;
    quint16 m_wipeBorderWidth;
    quint16 m_wipeBorderSoftness;
    quint8 m_wipeType;
    quint16 m_wipeSymmetry;
    quint16 m_wipeXPosition;
    quint16 m_wipeYPosition;
    bool m_wipeReverseDirection;
    bool m_wipeFlipFlop;

    quint8 m_dveFrames;

    quint8 m_stingFrames;

    quint8 m_borderSource;

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
    void downstreamKeyInvertKeyChanged(quint8 keyer, bool invert);
    void downstreamKeyPreMultipliedChanged(quint8 keyer, bool preMultiplied);
    void downstreamKeyClipChanged(quint8 keyer, float clip);
    void downstreamKeyGainChanged(quint8 keyer, float gain);
    void downstreamKeyEnableMaskChanged(quint8 keyer, bool enable);
    void downstreamKeyTopMaskChanged(quint8 keyer, float value);
    void downstreamKeyBottomMaskChanged(quint8 keyer, float value);
    void downstreamKeyLeftMaskChanged(quint8 keyer, float value);
    void downstreamKeyRightMaskChanged(quint8 keyer, float value);

    void upstreamKeyOnChanged(quint8 keyer, bool state);
    void upstreamKeyTypeChanged(quint8 keyer, quint8 type);
    void upstreamKeyFillSourceChanged(quint8 keyer, quint8 source);
    void upstreamKeyKeySourceChanged(quint8 keyer, quint8 source);
    void upstreamKeyEnableMaskChanged(quint8 keyer, bool enable);
    void upstreamKeyTopMaskChanged(quint8 keyer, qint16 value);
    void upstreamKeyBottomMaskChanged(quint8 keyer, qint16 value);
    void upstreamKeyLeftMaskChanged(quint8 keyer, qint16 value);
    void upstreamKeyRightMaskChanged(quint8 keyer, qint16 value);
    void upstreamKeyLumaPreMultipliedKeyChanged(quint8 keyer, bool preMultiplied);
    void upstreamKeyLumaInvertKeyChanged(quint8 keyer, bool invert);
    void upstreamKeyLumaClipChanged(quint8 keyer, quint16 clip);
    void upstreamKeyLumaGainChanged(quint8 keyer, quint16 gain);
    void upstreamKeyChromaHueChanged(quint8 keyer, quint16 hue);
    void upstreamKeyChromaGainChanged(quint8 keyer, quint16 gain);
    void upstreamKeyChromaYSuppressChanged(quint8 keyer, quint16 ySuppress);
    void upstreamKeyChromaLiftChanged(quint8 keyer, quint16 lift);
    void upstreamKeyChromaNarrowRangeChanged(quint8 keyer, bool narrowRange);
    void upstreamKeyPatternPatternChanged(quint8 keyer, quint8 pattern);
    void upstreamKeyPatternInvertPatternChanged(quint8 keyer, bool invert);
    void upstreamKeyPatternSize(quint8 keyer, quint16 size);
    void upstreamKeyPatternSymmetry(quint8 keyer, quint16 symmetry);
    void upstreamKeyPatternSoftness(quint8 keyer, quint16 softness);
    void upstreamKeyPatternXPosition(quint8 keyer, quint16 xPosition);
    void upstreamKeyPatternYPosition(quint8 keyer, quint16 yPosition);

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
    void wipeBorderWidthChanged(quint16 width);
    void wipeBorderSoftnessChanged(quint16 softness);
    void wipeTypeChanged(quint8 type);
    void wipeSymmetryChanged(quint16 value);
    void wipeXPositionChanged(quint16 value);
    void wipeYPositionChanged(quint16 value);
    void wipeReverseDirectionChanged(bool reverse);
    void wipeFlipFlopChanged(bool flipFlop);

    void dveFramesChanged(quint8 frames);

    void stingFramesChanged(quint8 frames);

    void borderSourceChanged(quint8 index);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAtemConnection::Commands)

#endif //QATEMCONNECTION_H
