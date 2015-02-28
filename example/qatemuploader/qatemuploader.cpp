#include "qatemuploader.h"

#include <qatemconnection.h>

#include <QTextStream>
#include <QCoreApplication>
#include <QHostAddress>
#include <QImage>
#include <QFileInfo>

QAtemUploader::QAtemUploader(QObject *parent) :
    QObject(parent), m_state(NotConnected), m_mediaplayer (-1)
{
    m_connection = new QAtemConnection(this);
    connect(m_connection, SIGNAL(socketError(QString)),
            this, SLOT(handleError(QString)));
    connect(m_connection, SIGNAL(connected()),
            this, SLOT(requestLock()));
    connect(m_connection, SIGNAL(mediaLockStateChanged(quint8,bool)),
            this, SLOT(handleMediaLockState(quint8,bool)));
    connect(m_connection, SIGNAL(dataTransferFinished(quint16)),
            this, SLOT(handleDataTransferFinished(quint16)));
}

void QAtemUploader::upload(const QString &filename, const QString &address, quint8 position)
{
    QHostAddress host(address);

    if(host.isNull())
    {
        handleError(tr("Invalid switcher address"));
        return;
    }

    m_connection->connectToSwitcher(host);

    m_filename = filename;
    m_position = position;
}

void QAtemUploader::handleError(const QString &errorString)
{
    QTextStream out(stderr);
    out << errorString << endl << endl;
    QCoreApplication::exit(-1);
}

void QAtemUploader::requestLock()
{
    m_state = AquiringLock;

    if(!m_connection->mediaLockState(0))
    {
        m_connection->aquireMediaLock(0, m_position);
    }
    else
    {
        QTextStream out(stdout);
        out << tr("Waiting for lock to be released... ");
    }
}

void QAtemUploader::handleMediaLockState(quint8 id, bool locked)
{
    QTextStream out(stdout);

    if (id == 0 && locked && m_state == AquiringLock)
    {
        QImage image(m_filename);

        if(image.isNull())
        {
            handleError(tr ("Failed to load image"));
            return;
        }

        QSize size = m_connection->availableVideoModes().value(m_connection->videoFormat()).size;

        QFileInfo info(m_filename);

        out << tr ("Uploading... ");
        m_connection->sendDataToSwitcher(0, m_position, info.baseName().toUtf8(), QAtemConnection::prepImageForSwitcher(image, size.width(), size.height()));
        m_state = Inprogress;
    }
    else if(id == 0 && !locked)
    {
        if(m_state == AquiringLock)
        {
            out << tr ("Trying") << endl;
            requestLock();
        }
        else if (m_state == Inprogress)
        {
            out << tr ("Done.") << endl << endl;
            m_state = Done;
            QCoreApplication::exit();
        }
    }
}

void QAtemUploader::handleDataTransferFinished(quint16 transferId)
{
    if(transferId == 1)
    {
        m_connection->unlockMediaLock(0);

        if(m_mediaplayer == 0 || m_mediaplayer == 1)
        {
            m_connection->setMediaPlayerSource(m_mediaplayer, false, m_position);
        }
    }
}
