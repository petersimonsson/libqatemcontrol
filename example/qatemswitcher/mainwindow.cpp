#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "qatemconnection.h"
#include "qatemmixeffect.h"

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

    m_programGroup = new QButtonGroup (this);
    m_programGroup->setExclusive(true);
    m_programGroup->addButton(m_ui->program1Button, 1);
    m_programGroup->addButton(m_ui->program2Button, 2);
    m_programGroup->addButton(m_ui->program3Button, 3);
    m_programGroup->addButton(m_ui->program4Button, 4);
    m_programGroup->addButton(m_ui->program5Button, 5);
    m_programGroup->addButton(m_ui->program6Button, 6);
    m_programGroup->addButton(m_ui->program7Button, 7);
    m_programGroup->addButton(m_ui->program8Button, 8);

    connect(m_programGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(changeProgramInput(int)));

    m_previewGroup = new QButtonGroup (this);
    m_previewGroup->setExclusive(true);
    m_previewGroup->addButton(m_ui->preview1Button, 1);
    m_previewGroup->addButton(m_ui->preview2Button, 2);
    m_previewGroup->addButton(m_ui->preview3Button, 3);
    m_previewGroup->addButton(m_ui->preview4Button, 4);
    m_previewGroup->addButton(m_ui->preview5Button, 5);
    m_previewGroup->addButton(m_ui->preview6Button, 6);
    m_previewGroup->addButton(m_ui->preview7Button, 7);
    m_previewGroup->addButton(m_ui->preview8Button, 8);

    connect(m_previewGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(changePreviewInput(int)));

    m_transitionStyleGroup = new QButtonGroup (this);
    m_transitionStyleGroup->setExclusive(true);
    m_transitionStyleGroup->addButton(m_ui->mixBtn, 0);
    m_transitionStyleGroup->addButton(m_ui->dipBtn, 1);
    m_transitionStyleGroup->addButton(m_ui->wipeBtn, 2);
    m_transitionStyleGroup->addButton(m_ui->dveBtn, 3);
    m_transitionStyleGroup->addButton(m_ui->stingBtn, 4);

    connect(m_transitionStyleGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(changeTransitionStyle(int)));

    m_keysTransitionGroup = new QButtonGroup (this);
    m_keysTransitionGroup->setExclusive(false);
    m_keysTransitionGroup->addButton(m_ui->bkgdBtn, 0);
    m_keysTransitionGroup->addButton(m_ui->key1Btn, 1);
    m_keysTransitionGroup->addButton(m_ui->key2Btn, 2);
    m_keysTransitionGroup->addButton(m_ui->key3Btn, 3);
    m_keysTransitionGroup->addButton(m_ui->key4Btn, 4);

    connect(m_keysTransitionGroup, SIGNAL(buttonToggled(int,bool)),
            this, SLOT(changeKeysTransition(int,bool)));

    m_keyOnAirGroup = new QButtonGroup(this);
    m_keyOnAirGroup->setExclusive(false);
    m_keyOnAirGroup->addButton(m_ui->key1OnAirBtn, 0);
    m_keyOnAirGroup->addButton(m_ui->key2OnAirBtn, 1);
    m_keyOnAirGroup->addButton(m_ui->key3OnAirBtn, 2);
    m_keyOnAirGroup->addButton(m_ui->key4OnAirBtn, 3);

    connect(m_keyOnAirGroup, SIGNAL(buttonToggled(int,bool)),
            this, SLOT(changeKeyOnAir(int,bool)));

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

    QAtemMixEffect *me = m_atemConnection->mixEffect(0);

    m_ui->key2Btn->setEnabled(me->upstreamKeyCount() >= 2);
    m_ui->key2OnAirBtn->setEnabled(me->upstreamKeyCount() >= 2);
    m_ui->key3Btn->setEnabled(me->upstreamKeyCount() >= 3);
    m_ui->key3OnAirBtn->setEnabled(me->upstreamKeyCount() >= 3);
    m_ui->key4Btn->setEnabled(me->upstreamKeyCount() >= 4);
    m_ui->key4OnAirBtn->setEnabled(me->upstreamKeyCount() >= 4);
    m_ui->stingBtn->setEnabled(m_atemConnection->topology().DVEs != 0);
    m_ui->dveBtn->setEnabled(m_atemConnection->topology().DVEs != 0);

    connect(m_ui->autoButton, SIGNAL(clicked()),
            me, SLOT(autoTransition()));
    connect(m_ui->cutButton, SIGNAL(clicked()),
            me, SLOT(cut()));

    connect(me, SIGNAL(programInputChanged(quint16,quint16)),
            this, SLOT(updateProgramInput(quint16,quint16)));
    connect(me, SIGNAL(previewInputChanged(quint16,quint16)),
            this, SLOT(updatePreviewInput(quint16,quint16)));

    connect(me, SIGNAL(transitionFrameCountChanged(quint8)),
            this, SLOT(setTransitionRate(quint8)));
    connect(me, SIGNAL(nextTransitionStyleChanged(quint8)),
            this, SLOT(setTransitionStyle(quint8)));
    connect(me, SIGNAL(keyersOnNextTransitionChanged(quint8)),
            this, SLOT(updateKeysOnNextTransition(quint8)));
    connect(me, SIGNAL(transitionPreviewChanged(bool)),
            m_ui->prevTransBtn, SLOT(setChecked(bool)));
    connect(m_ui->prevTransBtn, SIGNAL(toggled(bool)),
            me, SLOT(setTransitionPreview(bool)));
    connect(me, SIGNAL(transitionPositionChanged(quint16)),
            this, SLOT(setTransitionPosition(quint16)));
    connect(m_ui->tBar, SIGNAL(valueChanged(int)),
            this, SLOT(changeTransitionPosition(int)));

    connect(me, SIGNAL(fadeToBlackFrameCountChanged(quint8)),
            this, SLOT(setFadeToBlackRate(quint8)));
    connect(me, SIGNAL(fadeToBlackChanged(bool,bool)),
            this, SLOT(setFadeToBlack(bool,bool)));
    connect(m_ui->ftbBtn, SIGNAL(clicked()),
            me, SLOT(toggleFadeToBlack()));

    connect(me, SIGNAL(upstreamKeyOnAirChanged(quint8,bool)),
            this, SLOT(setUpstreamKeyOnAir(quint8,bool)));

    if(me->programInput() > 0 && me->programInput() <= 8)
    {
        m_programGroup->button(me->programInput())->setChecked(true);
    }
    if(me->previewInput() > 0 && me->previewInput() <= 8)
    {
        m_previewGroup->button(me->previewInput())->setChecked(true);
    }

    setTransitionRate(me->transitionFrameCount());
    setTransitionStyle(me->nextTransitionStyle());
    updateKeysOnNextTransition(me->keyersOnNextTransition());
    m_ui->prevTransBtn->setChecked(me->transitionPreviewEnabled());

    setFadeToBlackRate(me->fadeToBlackFrameCount());
    setFadeToBlack(me->fadeToBlackFading(), me->fadeToBlackEnabled());

    for(int i = 0; i < me->upstreamKeyCount(); ++i)
    {
        setUpstreamKeyOnAir(i, me->upstreamKeyOnAir(i));
    }
}

void MainWindow::changeProgramInput(int input)
{
    m_atemConnection->mixEffect(0)->changeProgramInput(input);
}

void MainWindow::changePreviewInput(int input)
{
    m_atemConnection->mixEffect(0)->changePreviewInput(input);
}

void MainWindow::updateProgramInput(quint16 oldInput, quint16 newInput)
{
    if(newInput > 0 && newInput <= 8)
    {
        m_programGroup->button(newInput)->setChecked(true);
    }
    else if(oldInput > 0 && oldInput <= 8)
    {
        m_programGroup->button(oldInput)->setChecked(false);
    }
}

void MainWindow::updatePreviewInput(quint16 oldInput, quint16 newInput)
{
    if(newInput > 0 && newInput <= 8)
    {
        m_previewGroup->button(newInput)->setChecked(true);
    }
    else if(oldInput > 0 && oldInput <= 8)
    {
        m_previewGroup->button(oldInput)->setChecked(false);
    }
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

void MainWindow::setTransitionRate(quint8 rate)
{
    m_ui->transitionRate->display(rate);
}

void MainWindow::setTransitionStyle(quint8 style)
{
    m_transitionStyleGroup->button(style)->setChecked(true);
}

void MainWindow::changeTransitionStyle(int style)
{
    m_atemConnection->mixEffect(0)->setTransitionType(style);
}

void MainWindow::updateKeysOnNextTransition(quint8 keyers)
{
    bool state = m_keysTransitionGroup->blockSignals(true);
    m_ui->bkgdBtn->setChecked(keyers & 1);
    m_ui->key1Btn->setChecked(keyers & (1 << 1));
    m_ui->key2Btn->setChecked(keyers & (1 << 2));
    m_ui->key3Btn->setChecked(keyers & (1 << 3));
    m_ui->key4Btn->setChecked(keyers & (1 << 4));
    m_keysTransitionGroup->blockSignals(state);
}

void MainWindow::changeKeysTransition(int btn, bool state)
{
    if(btn == 0)
    {
        m_atemConnection->mixEffect(0)->setBackgroundOnNextTransition(state);
    }
    else
    {
        m_atemConnection->mixEffect(0)->setUpstreamKeyOnNextTransition(btn - 1, state);
    }
}

void MainWindow::setTransitionPosition(quint16 pos)
{
    bool state = m_ui->tBar->blockSignals(true);

    if(m_ui->tBar->value() > 9000 && pos == 0)
    {
        m_ui->tBar->setInvertedAppearance(!m_ui->tBar->invertedAppearance());
    }

    m_ui->tBar->setValue(pos);
    m_ui->tBar->blockSignals(state);
}

void MainWindow::changeTransitionPosition(int pos)
{
    m_atemConnection->mixEffect(0)->setTransitionPosition(pos);
}

void MainWindow::setFadeToBlackRate(quint8 rate)
{
    m_ui->ftbRate->display(rate);
}

void MainWindow::setFadeToBlack(bool fading, bool state)
{
    m_ui->ftbBtn->setChecked(fading || state);
}

void MainWindow::setUpstreamKeyOnAir(quint8 key, bool state)
{
    bool block = m_keyOnAirGroup->blockSignals(true);
    m_keyOnAirGroup->button(key)->setChecked(state);
    m_keyOnAirGroup->blockSignals(block);
}

void MainWindow::changeKeyOnAir(int index, bool state)
{
    m_atemConnection->mixEffect(0)->setUpstreamKeyOnAir(index, state);
}
