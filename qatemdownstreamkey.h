#ifndef DOWNSTREAMKEY_H
#define DOWNSTREAMKEY_H

#include <QObject>

class QAtemConnection;

class QAtemDownstreamKey : public QObject
{
    Q_OBJECT
public:
    explicit QAtemDownstreamKey(quint8 id, QAtemConnection *parent);
    ~QAtemDownstreamKey();

    bool onAir() const { return m_onAir; }
    bool tie() const { return m_tie; }
    quint8 frameRate() const { return m_frameRate; }
    quint8 frameCount() const { return m_frameCount; }
    quint16 fillSource() const { return m_fillSource; }
    quint16 keySource() const { return m_keySource; }
    bool invertKey() const { return m_invertKey; }
    bool preMultiplied() const { return m_preMultiplied; }
    float clip() const { return m_clip; }
    float gain() const { return m_gain; }
    bool enableMask() const { return m_enableMask; }
    float topMask() const { return m_topMask; }
    float bottomMask() const { return m_bottomMask; }
    float leftMask() const { return m_leftMask; }
    float rightMask() const { return m_rightMask; }

public slots:
    void setOnAir(bool state);
    void setTie(bool state);
    void setFrameRate(quint8 frames);
    void setFillSource(quint16 source);
    void setKeySource(quint16 source);
    void setInvertKey(bool invert);
    void setPreMultiplied(bool preMultiplied);
    void setClip(float clip);
    void setGain(float gain);
    void setEnableMask(bool enabled);
    void setMask(float top, float bottom, float left, float right);

    void doAuto();

protected slots:
    void onDskS(const QByteArray &payload);
    void onDskP(const QByteArray &payload);
    void onDskB(const QByteArray &payload);

private:
    quint8 m_id;

    bool m_onAir;
    bool m_tie;
    quint8 m_frameRate;
    quint8 m_frameCount;
    quint16 m_fillSource;
    quint16 m_keySource;
    bool m_invertKey;
    bool m_preMultiplied;
    float m_clip;
    float m_gain;
    bool m_enableMask;
    float m_topMask;
    float m_bottomMask;
    float m_leftMask;
    float m_rightMask;

    QAtemConnection *m_atemConnection;

signals:
    void onAirChanged(quint8 keyer, bool state);
    void tieChanged(quint8 keyer, bool state);
    void frameCountChanged(quint8 keyer, quint8 count);
    void frameRateChanged(quint8 keyer, quint8 frames);
    void sourcesChanged(quint8 keyer, quint16 fill, quint16 key);
    void invertKeyChanged(quint8 keyer, bool invert);
    void preMultipliedChanged(quint8 keyer, bool preMultiplied);
    void clipChanged(quint8 keyer, float clip);
    void gainChanged(quint8 keyer, float gain);
    void enableMaskChanged(quint8 keyer, bool enable);
    void maskChanged(quint8 keyer, float top, float bottom, float left, float right);
};

#endif // DOWNSTREAMKEY_H
