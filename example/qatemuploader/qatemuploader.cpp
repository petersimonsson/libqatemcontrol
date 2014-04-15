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

        int width;
        int height;

        switch(m_connection->videoFormat())
        {
        case 0:
        case 2:
            width = 720;
            height = 525;
            break;
        case 1:
        case 3:
            width = 720;
            height = 625;
            break;
        case 4:
        case 5:
            width = 1280;
            height = 720;
            break;
        case 6:
        case 7:
            width = 1920;
            height = 1080;
            break;
        }

        QFileInfo info(m_filename);

        out << tr ("Uploading... ");
        m_connection->sendDataToSwitcher(0, m_position, info.baseName().toUtf8(), QAtemConnection::prepImageForSwitcher(image, width, height));
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
