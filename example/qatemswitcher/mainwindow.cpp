#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "qatemconnection.h"

#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    connect(m_ui->action_Quit, SIGNAL(triggered()),
            qApp, SLOT(quit()));

    m_atemConnection = new QAtemConnection(this);

    connect(m_atemConnection, SIGNAL(connected()),
            this, SLOT(onAtemConnected()));

    QButtonGroup *programGroup = new QButtonGroup (this);
    programGroup->addButton(m_ui->program1Button, 1);
    programGroup->addButton(m_ui->program2Button, 2);
    programGroup->addButton(m_ui->program3Button, 3);
    programGroup->addButton(m_ui->program4Button, 4);
    programGroup->addButton(m_ui->program5Button, 5);
    programGroup->addButton(m_ui->program6Button, 6);
    programGroup->addButton(m_ui->program7Button, 7);
    programGroup->addButton(m_ui->program8Button, 8);

    connect(programGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(changeProgramInput(int)));

    QButtonGroup *previewGroup = new QButtonGroup (this);
    previewGroup->addButton(m_ui->preview1Button, 1);
    previewGroup->addButton(m_ui->preview2Button, 2);
    previewGroup->addButton(m_ui->preview3Button, 3);
    previewGroup->addButton(m_ui->preview4Button, 4);
    previewGroup->addButton(m_ui->preview5Button, 5);
    previewGroup->addButton(m_ui->preview6Button, 6);
    previewGroup->addButton(m_ui->preview7Button, 7);
    previewGroup->addButton(m_ui->preview8Button, 8);

    connect(previewGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(changePreviewInput(int)));

    connect(m_ui->autoButton, SIGNAL(clicked()),
            m_atemConnection, SLOT(doAuto()));
    connect(m_ui->cutButton, SIGNAL(clicked()),
            m_atemConnection, SLOT(doCut()));

    connect(m_ui->dsk1TieButton, SIGNAL(clicked()),
            this, SLOT(toogleDsk1Tie()));
    connect(m_ui->dsk1OnAirButton, SIGNAL(clicked()),
            this, SLOT(toogleDsk1OnAir()));
    connect(m_ui->dsk1AutoButton, SIGNAL(clicked()),
            this, SLOT(doDsk1Auto()));

    connect(m_ui->dsk2TieButton, SIGNAL(clicked()),
            this, SLOT(toogleDsk2Tie()));
    connect(m_ui->dsk2OnAirButton, SIGNAL(clicked()),
            this, SLOT(toogleDsk2OnAir()));
    connect(m_ui->dsk2AutoButton, SIGNAL(clicked()),
            this, SLOT(doDsk2Auto()));

    connect(m_atemConnection, SIGNAL(downstreamKeyTieChanged(quint8,bool)),
            this, SLOT(updateDskTie(quint8,bool)));
    connect(m_atemConnection, SIGNAL(downstreamKeyOnChanged(quint8,bool)),
            this, SLOT(updateDskOn(quint8,bool)));

    connectToAtem();
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

void MainWindow::connectToAtem()
{
    QString address = QInputDialog::getText(this, tr("Connect To ATEM"), tr("Address:"));

    if(!address.isEmpty())
    {
        m_atemConnection->connectToSwitcher(QHostAddress(address));
    }
}

void MainWindow::onAtemConnected()
{
    quint8 count = 0;

    foreach(const QAtemConnection::InputInfo &info, m_atemConnection->inputInfos())
    {
        if(info.externalType != 0 /*Internal source*/)
        {
            ++count;
        }
    }

    m_ui->program7Button->setEnabled(count >= 7);
    m_ui->program8Button->setEnabled(count >= 8);
    m_ui->preview7Button->setEnabled(count >= 7);
    m_ui->preview8Button->setEnabled(count >= 8);
}

void MainWindow::changeProgramInput(int input)
{
    m_atemConnection->changeProgramInput(1, input);
}

void MainWindow::changePreviewInput(int input)
{
    m_atemConnection->changePreviewInput(1, input);
}

void MainWindow::toogleDsk1Tie()
{
    m_atemConnection->setDownstreamKeyTie(0, !m_atemConnection->downstreamKeyTie(0));
}

void MainWindow::toogleDsk1OnAir()
{
    m_atemConnection->setDownstreamKeyOn(0, !m_atemConnection->downstreamKeyOn(0));
}

void MainWindow::doDsk1Auto()
{
    m_atemConnection->doDownstreamKeyAuto(0);
}

void MainWindow::toogleDsk2Tie()
{
    m_atemConnection->setDownstreamKeyTie(1, !m_atemConnection->downstreamKeyTie(1));
}

void MainWindow::toogleDsk2OnAir()
{
    m_atemConnection->setDownstreamKeyOn(1, !m_atemConnection->downstreamKeyOn(1));
}

void MainWindow::doDsk2Auto()
{
    m_atemConnection->doDownstreamKeyAuto(1);
}

void MainWindow::updateDskTie(quint8 key, bool tie)
{
    if (key == 0)
    {
        m_ui->dsk1TieButton->setChecked(tie);
    }
    else if (key == 1)
    {
        m_ui->dsk2TieButton->setChecked(tie);
    }
}

void MainWindow::updateDskOn(quint8 key, bool on)
{
    if (key == 0)
    {
        m_ui->dsk1OnAirButton->setChecked(on);
    }
    else if (key == 1)
    {
        m_ui->dsk2OnAirButton->setChecked(on);
    }
}
