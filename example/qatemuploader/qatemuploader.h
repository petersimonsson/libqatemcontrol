#ifndef QATEMUPLOADER_H
#define QATEMUPLOADER_H

#include <QObject>

class QAtemConnection;

class QAtemUploader : public QObject
{
    Q_OBJECT
public:
    enum State {
        NotConnected,
        AquiringLock,
        Inprogress,
        Done
    };

    explicit QAtemUploader(QObject *parent = 0);

    void setMediaPlayer(qint8 id) { m_mediaplayer = id; }
    void upload(const QString &filename, const QString &address, quint8 position);

protected slots:
    void handleError(const QString &errorString);

    void requestLock();
    void handleMediaLockState(quint8 id, bool locked);

    void handleDataTransferFinished(quint16 transferId);

private:
    QAtemConnection *m_connection;

    QString m_filename;
    quint8 m_position;
    State m_state;
    qint8 m_mediaplayer;
};

#endif // QATEMUPLOADER_H
