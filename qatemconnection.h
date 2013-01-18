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

#ifndef QATEMCONNECTION_H
#define QATEMCONNECTION_H
#include "qdownstreamkeysettings.h"
#include "qupstreamkeysettings.h"

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

    void setDebugEnabled(bool enabled) { m_debugEnabled = enabled; }
    bool debugEnabled() const { return m_debugEnabled; }

    /// @returns the index of the input that is on program
    quint8 programInput() const { return m_programInput; }
    /// @returns the index of the input that is on preview
    quint8 previewInput() const { return m_previewInput; }

    /// @returns the tally state of the input @p id. 1 = program, 2 = preview and 3 = both
    quint8 tallyState(quint8 id) const;
    /// @returns number of tally states
    quint8 tallyStateCount() const { return m_tallyStateCount; }

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
    quint8 downstreamKeyFrameCount(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_frameCount; }
    /// @returns duration in number of frames for key transition of dsk @p keyer
    quint8 downstreamKeyFrames(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_frames; }
    /// @returns the input selected as fill source for downstream key @p keyer
    quint8 downstreamKeyFillSource(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_fillSource; }
    /// @returns the input selected as key source for downstream key @p keyer
    quint8 downstreamKeyKeySource(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_keySource; }
    /// @returns true if the should be inverted for downstream key @p keyer
    bool downstreamKeyInvertKey(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_keySource; }
    /// @returns true if the key is pre multiplied for downstream key @p keyer
    bool donwstreamKeyPreMultiplied(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_preMultiplied; }
    /// @returns the clip set for downstream key @p keyer
    quint16 downstreamKeyClip(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_clip; }
    /// @returns the gain set for downstream key @p keyer
    quint16 downstreamKeyGain(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_gain; }
    /// @returns true if the mask for downstream key @p keyer is enabled
    bool downstreamKeyEnableMask(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_enableMask; }
    /// @returns the top position of the mask for downstream key @p keyer
    qint16 downstreamKeyTopMask(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_topMask; }
    /// @returns the bottom position of the mask for downstream key @p keyer
    qint16 downstreamKeyBottomMask(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_bottomMask; }
    /// @returns the left position of the mask for downstream key @p keyer
    qint16 downstreamKeyLeftMask(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_leftMask; }
    /// @returns the right position of the mask for downstream key @p keyer
    qint16 downstreamKeyRightMask(quint8 keyer) const { return m_downstreamKeys.value(keyer).m_rightMask; }

    /// @returns true if upstream key @p keyer is on air
    bool upstreamKeyOn(quint8 keyer) const;
    /// @returns the key type for upstream key @p keyer, 0 = luma, 1 = chroma, 2 = pattern, 3 = DVE
    quint8 upstreamKeyType(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_type; }
    /// @returns the source used as fill for upstream key @p keyer
    quint8 upstreamKeyFillSource(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_fillSource; }
    /// @returns the source used as key for upstream key @p keyer
    quint8 upstreamKeyKeySource(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_keySource; }
    /// @returns true if the mask is enabled for upstream key @p keyer
    bool upstreamKeyEnableMask(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_enableMask; }
    /// @returns top mask for upstream key @p keyer
    float upstreamKeyTopMask(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_topMask; }
    /// @returns bottom mask for upstream key @p keyer
    float upstreamKeyBottomMask(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_bottomMask; }
    /// @returns left mask for upstream key @p keyer
    float upstreamKeyLeftMask(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_leftMask; }
    /// @returns right mask for upstream key @p keyer
    float upstreamKeyRightMask(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_rightMask; }
    /// @returns true if the key is pre multiplied for luma upstream key @p keyer
    bool upstreamKeyLumaPreMultipliedKey(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_lumaPreMultipliedKey; }
    /// @returns true if the key source should be inverted for luma upstream key @p keyer
    bool upstreamKeyLumaInvertKey(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_lumaInvertKey; }
    /// @returns clip for luma upstream key @p keyer
    float upstreamKeyLumaClip(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_lumaClip; }
    /// @returns gain for luma upstream key @p keyer
    float upstreamKeyLumaGain(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_lumaGain; }
    /// @returns hue for chroma upstream key @p keyer
    float upstreamKeyChromaHue(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_chromaHue; }
    /// @returns gain for chroma upstream key @p keyer
    float upstreamKeyChromaGain(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_chromaGain; }
    /// @returns y suppress for chroma upstream key @p keyer
    float upstreamKeyChromaYSuppress(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_chromaYSuppress; }
    /// @returns lift for chroma upstream key @p keyer
    float upstreamKeyChromaLift(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_chromaLift; }
    /// @returns true if chroma upstream key @p keyer should have narrow chroma key range
    bool upstreamKeyChromaNarrowRange(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_chromaNarrowRange; }
    /// @returns pattern of pattern upstream key @p keyer
    quint8 upstreamKeyPatternPattern(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_patternPattern; }
    /// @returns true if pattern upstream key @p keyer should invert the pattern
    bool upstreamKeyPatternInvertPattern(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_patternInvertPattern; }
    /// @returns size for pattern upstream key @p keyer
    float upstreamKeyPatternSize(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_patternSize; }
    /// @returns symmetry for pattern upstream key @p keyer
    float upstreamKeyPatternSymmetry(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_patternSymmetry; }
    /// @returns softness for pattern upstream key @p keyer
    float upstreamKeyPatternSoftness(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_patternSoftness; }
    /// @returns x position for pattern upstream key @p keyer
    float upstreamKeyPatternXPosition(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_patternXPosition; }
    /// @returns y position for pattern upstream key @p keyer
    float upstreamKeyPatternYPosition(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_patternYPosition; }
    /// @returns x position of DVE for upstream key @p keyer
    float upstreamKeyDVEXPosition(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveXPosition; }
    /// @returns y position of DVE for upstream key @p keyer
    float upstreamKeyDVEYPosition(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveYPosition; }
    /// @returns x size of DVE for upstream key @p keyer
    float upstreamKeyDVEXSize(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveXSize; }
    /// @returns y size of DVE for upstream key @p keyer
    float upstreamKeyDVEYSize(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveYSize; }
    /// @returns rotation of DVE for upstream key @p keyer
    float upstreamKeyDVERotation(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveRotation; }
    /// @returns true if the drop shadow is enabled on the DVE for upstream key @p keyer
    bool upstreamKeyDVEDropShadowEnabled(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveEnableDropShadow; }
    /// @returns direction of the light source for the drop shadow on the DVE for upstream key @p keyer
    float upstreamKeyDVELightSourceDirection(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveLightSourceDirection; }
    /// @returns altitude of the light source for the drop shadow on the DVE for upstream keu @p keyer
    quint8 upstreamKeyDVELightSourceAltitude(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveLightSourceAltitude; }
    /// @returns true if the border is enabled on the DVE for upstream key @p keyer
    bool upstreamKeyDVEBorderEnabled(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveEnableBorder; }
    /// @returns the border style of the DVE for upstream key @p keyer. 0 = No Bevel, 1 = Bevel In Out, 2 = Bevel In, 3 = Bevel Out
    quint8 upstreamKeyDVEBorderStyle(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderStyle; }
    /// @returns the border color of the DVE for upstream key @p keyer
    QColor upstreamKeyDVEBorderColor(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderColor; }
    /// @returns the outside width of the border of the DVE for upstream key @p keyer
    float upstreamKeyDVEBorderOutsideWidth(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderOutsideWidth; }
    /// @returns the inside width of the border of the DVE for upstream key @p keyer
    float upstreamKeyDVEBorderInsideWidth(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderInsideWidth; }
    /// @returns the outside soften (%) of the border of the DVE for upstream key @p keyer
    quint8 upstreamKeyDVEBorderOutsideSoften(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderOutsideSoften; }
    /// @returns the inside soften (%) of the border of the DVE for upstream key @p keyer
    quint8 upstreamKeyDVEBorderInsideSoften(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderInsideSoften; }
    /// @returns the opacity of the border of the DVE for upstream key @p keyer
    quint8 upstreamKeyDVEBorderOpacity(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderOpacity; }
    /// @returns the bevel position of the border of the DVE for upstream key @p keyer
    float upstreamKeyDVEBorderBevelPosition(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderBevelPosition; }
    /// @returns the bevel soften (%) of the border of the DVE for upstream key @p keyer
    quint8 upstreamKeyDVEBorderBevelSofter(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveBorderBevelSoften; }
    /// @returns the rate in frames the DVE for upstream key @p keyer runs at
    quint8 upstreamKeyDVERate(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveRate; }
    /// @returns true if key frame A has been set for the DVE for upstream key @p keyer
    bool upstreamKeyDVEKeyFrameASet(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveKeyFrameASet; }
    /// @returns true if key frame B has been set for the DVE for upstream key @p keyer
    bool upstreamKeyDVEKeyFrameBSet(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_dveKeyFrameBSet; }
    /// @returns true if fly is enabled for non DVE type of the upstream key @p keyer
    bool upstreamKeyEnableFly(quint8 keyer) const { return m_upstreamKeys.value(keyer).m_enableFly; }

    QColor colorGeneratorColor(quint8 generator) const;

    quint8 mediaPlayerType(quint8 player) const;
    quint8 mediaPlayerSelectedStill(quint8 player) const;
    quint8 mediaPlayerSelectedClip(quint8 player) const;

    quint8 auxSource(quint8 aux) const;

    QString productInformation() const { return m_productInformation; }
    quint16 majorVersion() const { return m_majorversion; }
    quint16 minorVersion() const { return m_minorversion; }

    /// @returns Info about the input @p index
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

    /// @returns duration in number of frames for DVE transition
    quint16 dveRate() const { return m_dveRate; }
    /**
     * @returns the selected effect for DVE transition
     *
     * Swosh:
     * 0 = Top left
     * 1 = Up
     * 2 = Top right
     * 3 = Left
     * 4 = Right
     * 5 = Bottom left
     * 6 = Down
     * 7 = Bottom right
     *
     * Spin:
     * 8 = Down left, clockwise
     * 9 = Up left, clockwise
     * 10 = Down right, clockwise
     * 11 = Up right, clockwise
     * 12 = Up right, anti clockwise
     * 13 = Down right, anti clockwise
     * 14 = Up left, anti clockwise
     * 15 = Down left, anti clockwise
     *
     * Squeeze:
     * 16 = Top left
     * 17 = Up
     * 18 = Top right
     * 19 = Left
     * 20 = Right
     * 21 = Bottom left
     * 22 = Down
     * 23 = Bottom right
     *
     * Push:
     * 24 = Top left
     * 25 = Up
     * 26 = Top right
     * 27 = Left
     * 28 = Right
     * 29 = Bottom left
     * 30 = Down
     * 31 = Bottom right
     *
     * Graphic:
     * 32 = Spin clockwise
     * 33 = Spin anti clockwise
     * 34 = Logo wipe
     */
    quint8 dveEffect() const { return m_dveEffect; }
    quint8 dveFillSource() const { return m_dveFillSource; }
    quint8 dveKeySource() const { return m_dveKeySource; }
    bool dveKeyEnabled() const { return m_dveEnableKey; }
    bool dvePreMultipliedKeyEnabled() const { return m_dveEnablePreMultipliedKey; }
    /// @returns the clip of the key in per cent for the DVE transition
    float dveKeyClip() const { return m_dveKeyClip; }
    /// @returns the gain of the key in per cent for the DVE transition
    float dveKeyGain() const { return m_dveKeyGain; }
    bool dveInvertKeyEnabled() const { return m_dveEnableInvertKey; }

    /// @returns source used for the Stinger transition. 1 = Media player 1, 2 = Media player 2
    quint8 stingerSource() const { return m_stingerSource; }
    /// @returns true if the Stinger transition has a pre multiplied key
    bool stingerPreMultipliedKeyEnabled() const { return m_stingerEnablePreMultipliedKey; }
    float stingerClip() const { return m_stingerClip; }
    float stingerGain() const { return m_stingerGain; }
    bool stingerInvertKeyEnabled() const { return m_stingerEnableInvertKey; }
    quint16 stingerPreRoll() const { return m_stingerPreRoll; }
    quint16 stingerClipDuration() const { return m_stingerClipDuration; }
    quint16 stingerTriggerPoint() const { return m_stingerTriggerPoint; }
    quint16 stingerMixRate() const { return m_stingerMixRate; }

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
    void setUpstreamKeyTopMask(quint8 keyer, float value);
    void setUpstreamKeyBottomMask(quint8 keyer, float value);
    void setUpstreamKeyLeftMask(quint8 keyer, float value);
    void setUpstreamKeyRightMask(quint8 keyer, float value);
    void setUpstreamKeyLumaPreMultipliedKey(quint8 keyer, bool preMultiplied);
    void setUpstreamKeyLumaInvertKey(quint8 keyer, bool invert);
    void setUpstreamKeyLumaClip(quint8 keyer, float clip);
    void setUpstreamKeyLumaGain(quint8 keyer, float gain);
    void setUpstreamKeyChromaHue(quint8 keyer, float hue);
    void setUpstreamKeyChromaGain(quint8 keyer, float gain);
    void setUpstreamKeyChromaYSuppress(quint8 keyer, float ySuppress);
    void setUpstreamKeyChromaLift(quint8 keyer, float lift);
    void setUpstreamKeyChromaNarrowRange(quint8 keyer, bool narrowRange);
    void setUpstreamKeyPatternPattern(quint8 keyer, quint8 pattern);
    void setUpstreamKeyPatternInvertPattern(quint8 keyer, bool invert);
    void setUpstreamKeyPatternSize(quint8 keyer, float size);
    void setUpstreamKeyPatternSymmetry(quint8 keyer, float symmetry);
    void setUpstreamKeyPatternSoftness(quint8 keyer, float softness);
    void setUpstreamKeyPatternXPosition(quint8 keyer, float xPosition);
    void setUpstreamKeyPatternYPosition(quint8 keyer, float yPosition);
    void setUpstreamKeyDVEPosition(quint8 keyer, float xPosition, float yPosition);
    void setUpstreamKeyDVESize(quint8 keyer, float xSize, float ySize);
    void setUpstreamKeyDVERotation(quint8 keyer, float rotation);
    void setUpstreamKeyDVELightSource(quint8 keyer, float direction, quint8 altitude);
    void setUpstreamKeyDVEDropShadowEnabled(quint8 keyer, bool enabled);
    void setUpstreamKeyDVEBorderEnabled(quint8 keyer, bool enabled);
    /// Set the border style of the upstream key DVE. 0 = No Bevel, 1 = Bevel In Out, 2 = Bevel In, 3 = Bevel Out
    void setUpstreamKeyDVEBorderStyle(quint8 keyer, quint8 style);
    void setUpstreamKeyDVEBorderColorH(quint8 keyer, float h);
    void setUpstreamKeyDVEBorderColorS(quint8 keyer, float s);
    void setUpstreamKeyDVEBorderColorL(quint8 keyer, float l);
    void setUpstreamKeyDVEBorderColor(quint8 keyer, const QColor& color);
    void setUpstreamKeyDVEBorderWidth(quint8 keyer, float outside, float inside);
    void setUpstreamKeyDVEBorderSoften(quint8 keyer, quint8 outside, quint8 inside);
    void setUpstreamKeyDVEBorderOpacity(quint8 keyer, quint8 opacity);
    void setUpstreamKeyDVEBorderBevelPosition(quint8 keyer, float position);
    void setUpstreamKeyDVEBorderBevelSoften(quint8 keyer, quint8 soften);
    void setUpstreamKeyDVERate(quint8 keyer, quint8 rate);
    /// Set the @p keyFrame of the DVE for upstream keyer @p keyer. 1 = Keyframe A, 2 = Keyframe B
    void setUpstreamKeyDVEKeyFrame(quint8 keyer, quint8 keyFrame);
    /**
     * Make the upstream key @p keyer run to @p position. 1 = Keyframe A, 2 = Keyframe B, 3 = Fullscreen, 4 = Infinite
     * If the @p position = infinite there is also a @p direction.
     * Available directions:
     * 0 = Center
     * 1 = Top left
     * 2 = Up
     * 3 = Top right
     * 4 = Left
     * 5 = Center
     * 6 = Right
     * 7 = Bottom left
     * 8 = Down
     * 9 = Bottom right
     */
    void runUpstreamKeyTo(quint8 keyer, quint8 position, quint8 direction);
    /// Enable fly on the non DVE key types of the upstream key @p keyer
    void setUpstreamKeyFlyEnabled(quint8 keyer, bool enable);

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

    void setDVERate(quint16 frames);
    /**
     * Set the effect used for the DVE transition to @p effect
     *
     * Swosh:
     * 0 = Top left
     * 1 = Up
     * 2 = Top right
     * 3 = Left
     * 4 = Right
     * 5 = Bottom left
     * 6 = Down
     * 7 = Bottom right
     *
     * Spin:
     * 8 = Down left, clockwise
     * 9 = Up left, clockwise
     * 10 = Down right, clockwise
     * 11 = Up right, clockwise
     * 12 = Up right, anti clockwise
     * 13 = Down right, anti clockwise
     * 14 = Up left, anti clockwise
     * 15 = Down left, anti clockwise
     *
     * Squeeze:
     * 16 = Top left
     * 17 = Up
     * 18 = Top right
     * 19 = Left
     * 20 = Right
     * 21 = Bottom left
     * 22 = Down
     * 23 = Bottom right
     *
     * Push:
     * 24 = Top left
     * 25 = Up
     * 26 = Top right
     * 27 = Left
     * 28 = Right
     * 29 = Bottom left
     * 30 = Down
     * 31 = Bottom right
     *
     * Graphic:
     * 32 = Spin clockwise
     * 33 = Spin anti clockwise
     * 34 = Logo wipe
     */
    void setDVEEffect(quint8 effect);
    void setDVEFillSource(quint8 source);
    void setDVEKeySource(quint8 source);
    void setDVEKeyEnabled(bool enabled);
    void setDVEPreMultipliedKeyEnabled(bool enabled);
    /// Set clip of key for DVE transition to @p percent
    void setDVEKeyClip(float percent);
    /// Set gain of key for DVE transition to @p percent
    void setDVEKeyGain(float percent);
    void setDVEInvertKeyEnabled(bool enabled);

    /// Set the source used for Stinger transition to @p source. 1 = Media player 1, 2 = Media player 2
    void setStingerSource(quint8 source);
    /// Enable if the key is pre multiplied in the source for the Stinger transition
    void setStingerPreMultipliedKeyEnabled(bool enabled);
    void setStingerClip(float percent);
    void setStingerGain(float percent);
    void setStingerInvertKeyEnabled(bool enabled);
    void setStingerPreRoll(quint16 frames);
    void setStingerClipDuration(quint16 frames);
    void setStingerTriggerPoint(quint16 frames);
    void setStingerMixRate(quint16 frames);

    void setAuxSource(quint8 aux, quint8 source);

    void setInputType(quint8 input, quint8 type);
    void setInputLongName(quint8 input, const QString& name);
    void setInputShortName(quint8 input, const QString& name);

    void setVideoFormat(quint8 format);

    void setMultiViewLayout(quint8 layout);

protected slots:
    void handleSocketData();

    void handleError(QAbstractSocket::SocketError);

    void onPrgI(const QByteArray& payload);
    void onPrvI(const QByteArray& payload);
    void onTlIn(const QByteArray& payload);
    void onTrPr(const QByteArray& payload);
    void onTrPs(const QByteArray& payload);
    void onTrSS(const QByteArray& payload);
    void onFtbS(const QByteArray& payload);
    void onFtbP(const QByteArray& payload);
    void onDskS(const QByteArray& payload);
    void onDskP(const QByteArray& payload);
    void onDskB(const QByteArray& payload);
    void onKeOn(const QByteArray& payload);
    void onColV(const QByteArray& payload);
    void onMPCE(const QByteArray& payload);
    void onAuxS(const QByteArray& payload);
    void on_pin(const QByteArray& payload);
    void on_ver(const QByteArray& payload);
    void onInPr(const QByteArray& payload);
    void onMPSE(const QByteArray& payload);
    void onMvIn(const QByteArray& payload);
    void onMvPr(const QByteArray& payload);
    void onVidM(const QByteArray& payload);
    void onTime(const QByteArray& payload);
    void onTMxP(const QByteArray& payload);
    void onTDpP(const QByteArray& payload);
    void onTWpP(const QByteArray& payload);
    void onTDvP(const QByteArray& payload);
    void onTStP(const QByteArray& payload);
    void onBrdI(const QByteArray& payload);
    void onKeBP(const QByteArray& payload);
    void onKeLm(const QByteArray& payload);
    void onKeCk(const QByteArray& payload);
    void onKePt(const QByteArray& payload);
    void onKeDV(const QByteArray& payload);
    void onKeFS(const QByteArray& payload);

protected:
    QByteArray createCommandHeader(Commands bitmask, quint16 payloadSize, quint16 uid, quint16 ackId, quint16 undefined1, quint16 undefined2);

    QAtemConnection::CommandHeader parseCommandHeader(const QByteArray& datagram) const;
    void parsePayLoad(const QByteArray& datagram);

    void sendDatagram(const QByteArray& datagram);
    void sendCommand(const QByteArray& cmd, const QByteArray &payload);

    void initCommandSlotHash();

    void setKeyOnNextTransition (int index, bool state);

private:
    QUdpSocket* m_socket;

    QHostAddress m_address;
    quint16 m_port;

    quint16 m_packetCounter;
    bool m_isInitialized;
    quint16 m_currentUid;

    QHash<QByteArray, QByteArray> m_commandSlotHash;

    bool m_debugEnabled;

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

    QHash<quint8, QDownstreamKeySettings> m_downstreamKeys;
    QHash<quint8, QUpstreamKeySettings> m_upstreamKeys;

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

    quint16 m_dveRate;
    quint8 m_dveEffect;
    quint8 m_dveFillSource;
    quint8 m_dveKeySource;
    bool m_dveEnableKey;
    bool m_dveEnablePreMultipliedKey;
    float m_dveKeyClip;
    float m_dveKeyGain;
    bool m_dveEnableInvertKey;

    quint8 m_stingerSource;
    bool m_stingerEnablePreMultipliedKey;
    float m_stingerClip;
    float m_stingerGain;
    bool m_stingerEnableInvertKey;
    quint16 m_stingerPreRoll;
    quint16 m_stingerClipDuration;
    quint16 m_stingerTriggerPoint;
    quint16 m_stingerMixRate;

    quint8 m_borderSource;

signals:
    void connected();
    void socketError(const QString& errorString);

    void programInputChanged(quint8 oldIndex, quint8 newIndex);
    void previewInputChanged(quint8 oldIndex, quint8 newIndex);

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
    void upstreamKeyTopMaskChanged(quint8 keyer, float value);
    void upstreamKeyBottomMaskChanged(quint8 keyer, float value);
    void upstreamKeyLeftMaskChanged(quint8 keyer, float value);
    void upstreamKeyRightMaskChanged(quint8 keyer, float value);
    void upstreamKeyLumaPreMultipliedKeyChanged(quint8 keyer, bool preMultiplied);
    void upstreamKeyLumaInvertKeyChanged(quint8 keyer, bool invert);
    void upstreamKeyLumaClipChanged(quint8 keyer, float clip);
    void upstreamKeyLumaGainChanged(quint8 keyer, float gain);
    void upstreamKeyChromaHueChanged(quint8 keyer, float hue);
    void upstreamKeyChromaGainChanged(quint8 keyer, float gain);
    void upstreamKeyChromaYSuppressChanged(quint8 keyer, float ySuppress);
    void upstreamKeyChromaLiftChanged(quint8 keyer, float lift);
    void upstreamKeyChromaNarrowRangeChanged(quint8 keyer, bool narrowRange);
    void upstreamKeyPatternPatternChanged(quint8 keyer, quint8 pattern);
    void upstreamKeyPatternInvertPatternChanged(quint8 keyer, bool invert);
    void upstreamKeyPatternSizeChanged(quint8 keyer, float size);
    void upstreamKeyPatternSymmetryChanged(quint8 keyer, float symmetry);
    void upstreamKeyPatternSoftnessChanged(quint8 keyer, float softness);
    void upstreamKeyPatternXPositionChanged(quint8 keyer, float xPosition);
    void upstreamKeyPatternYPositionChanged(quint8 keyer, float yPosition);
    void upstreamKeyDVEXPositionChanged(quint8 keyer, float xPosition);
    void upstreamKeyDVEYPositionChanged(quint8 keyer, float yPosition);
    void upstreamKeyDVEXSizeChanged(quint8 keyer, float xSize);
    void upstreamKeyDVEYSizeChanged(quint8 keyer, float ySize);
    void upstreamKeyDVERotationChanged(quint8 keyer, float rotation);
    void upstreamKeyDVEEnableDropShadowChanged(quint8 keyer, bool enable);
    void upstreamKeyDVELighSourceDirectionChanged(quint8 keyer, float direction);
    void upstreamKeyDVELightSourceAltitudeChanged(quint8 keyer, quint8 altitude);
    void upstreamKeyDVEEnableBorderChanged(quint8 keyer, bool enable);
    void upstreamKeyDVEBorderStyleChanged(quint8 keyer, quint8 style);
    void upstreamKeyDVEBorderColorChanged(quint8 keyer, QColor color);
    void upstreamKeyDVEBorderOutsideWidthChanged(quint8 keyer, float width);
    void upstreamKeyDVEBorderInsideWidthChanged(quint8 keyer, float width);
    void upstreamKeyDVEBorderOutsideSoftenChanged(quint8 keyer, quint8 soften);
    void upstreamKeyDVEBorderInsideSoftenChanged(quint8 keyer, quint8 soften);
    void upstreamKeyDVEBorderOpacityChanged(quint8 keyer, quint8 opacity);
    void upstreamKeyDVERateChanged(quint8 keyer, quint8 rate);
    void upstreamKeyDVEKeyFrameASetChanged(quint8 keyer, bool set);
    void upstreamKeyDVEKeyFrameBSetChanged(quint8 keyer, bool set);
    void upstreamKeyEnableFlyChanged(quint8 keyer, bool enabled);

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

    void dveRateChanged(quint16 frames);
    void dveEffectChanged(quint8 effect);
    void dveFillSourceChanged(quint8 source);
    void dveKeySourceChanged(quint8 source);
    void dveEnableKeyChanged(bool enabled);
    void dveEnablePreMultipliedKeyChanged(bool enabled);
    void dveKeyClipChanged(float clip);
    void dveKeyGainChanged(float gain);
    void dveEnableInvertKeyChanged(bool enabled);

    void stingerSourceChanged(quint8 frames);
    void stingerEnablePreMultipliedKeyChanged(bool enabled);
    void stingerClipChanged(float percent);
    void stingerGainChanged(float percent);
    void stingerEnableInvertKeyChanged(bool enabled);
    void stingerPreRollChanged(quint16 frames);
    void stingerClipDurationChanged(quint16 frames);
    void stingerTriggerPointChanged(quint16 frames);
    void stingerMixRateChanged(quint16 frames);

    void borderSourceChanged(quint8 index);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAtemConnection::Commands)

#endif //QATEMCONNECTION_H
