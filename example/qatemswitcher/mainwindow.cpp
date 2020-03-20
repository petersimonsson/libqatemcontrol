#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "qatemconnection.h"
#include "qatemmixeffect.h"
#include "qatemdownstreamkey.h"

#include <QInputDialog>
#include <QButtonGroup>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    connect(m_ui->action_Quit, SIGNAL(triggered()),
            qApp, SLOT(quit()));

    m_atemConnection = new QAtemConnection(this);

    connect(m_atemConnection, &QAtemConnection::connected,
            this, &MainWindow::onAtemConnected);

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

    connect(m_programGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MainWindow::changeProgramInput);

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

    connect(m_previewGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MainWindow::changePreviewInput);

    m_transitionStyleGroup = new QButtonGroup (this);
    m_transitionStyleGroup->setExclusive(true);
    m_transitionStyleGroup->addButton(m_ui->mixBtn, 0);
    m_transitionStyleGroup->addButton(m_ui->dipBtn, 1);
    m_transitionStyleGroup->addButton(m_ui->wipeBtn, 2);
    m_transitionStyleGroup->addButton(m_ui->dveBtn, 3);
    m_transitionStyleGroup->addButton(m_ui->stingBtn, 4);

    connect(m_transitionStyleGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MainWindow::changeTransitionStyle);

    m_keysTransitionGroup = new QButtonGroup (this);
    m_keysTransitionGroup->setExclusive(false);
    m_keysTransitionGroup->addButton(m_ui->bkgdBtn, 0);
    m_keysTransitionGroup->addButton(m_ui->key1Btn, 1);
    m_keysTransitionGroup->addButton(m_ui->key2Btn, 2);
    m_keysTransitionGroup->addButton(m_ui->key3Btn, 3);
    m_keysTransitionGroup->addButton(m_ui->key4Btn, 4);

    connect(m_keysTransitionGroup, QOverload<int, bool>::of(&QButtonGroup::buttonToggled),
            this, &MainWindow::changeKeysTransition);

    m_keyOnAirGroup = new QButtonGroup(this);
    m_keyOnAirGroup->setExclusive(false);
    m_keyOnAirGroup->addButton(m_ui->key1OnAirBtn, 0);
    m_keyOnAirGroup->addButton(m_ui->key2OnAirBtn, 1);
    m_keyOnAirGroup->addButton(m_ui->key3OnAirBtn, 2);
    m_keyOnAirGroup->addButton(m_ui->key4OnAirBtn, 3);

    connect(m_keyOnAirGroup, QOverload<int, bool>::of(&QButtonGroup::buttonToggled),
            this, &MainWindow::changeKeyOnAir);

    connect(m_ui->dsk1TieButton, &QPushButton::clicked,
            this, &MainWindow::toogleDsk1Tie);
    connect(m_ui->dsk1OnAirButton, &QPushButton::clicked,
            this, &MainWindow::toogleDsk1OnAir);
    connect(m_ui->dsk1AutoButton, SIGNAL(clicked()),
            this, SLOT(doDsk1Auto()));
    connect(m_ui->dsk1AutoButton, &QPushButton::clicked,
            this, &MainWindow::doDsk1Auto);

    connect(m_ui->dsk2TieButton, &QPushButton::clicked,
            this, &MainWindow::toogleDsk2Tie);
    connect(m_ui->dsk2OnAirButton, &QPushButton::clicked,
            this, &MainWindow::toogleDsk2OnAir);
    connect(m_ui->dsk2AutoButton, &QPushButton::clicked,
            this, &MainWindow::doDsk2Auto);

    foreach(QAtemDownstreamKey *dsk, m_atemConnection->downstreamKeys())
    {
        connect(dsk, &QAtemDownstreamKey::tieChanged,
                this, &MainWindow::updateDskTie);
        connect(dsk, &QAtemDownstreamKey::onAirChanged,
                this, &MainWindow::updateDskOn);
    }

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

    foreach(const QAtem::InputInfo &info, m_atemConnection->inputInfos())
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

    if(me)
    {
        m_ui->key2Btn->setEnabled(me->upstreamKeyCount() >= 2);
        m_ui->key2OnAirBtn->setEnabled(me->upstreamKeyCount() >= 2);
        m_ui->key3Btn->setEnabled(me->upstreamKeyCount() >= 3);
        m_ui->key3OnAirBtn->setEnabled(me->upstreamKeyCount() >= 3);
        m_ui->key4Btn->setEnabled(me->upstreamKeyCount() >= 4);
        m_ui->key4OnAirBtn->setEnabled(me->upstreamKeyCount() >= 4);
        m_ui->stingBtn->setEnabled(m_atemConnection->topology().DVEs != 0);
        m_ui->dveBtn->setEnabled(m_atemConnection->topology().DVEs != 0);

        connect(m_ui->autoButton, &QPushButton::clicked,
                me, &QAtemMixEffect::autoTransition);
        connect(m_ui->cutButton, &QPushButton::clicked,
                me, &QAtemMixEffect::cut);

        connect(me, &QAtemMixEffect::programInputChanged,
                this, &MainWindow::updateProgramInput);
        connect(me, &QAtemMixEffect::previewInputChanged,
                this, &MainWindow::updatePreviewInput);

        connect(me, &QAtemMixEffect::transitionFrameCountChanged,
                this, &MainWindow::setTransitionRate);
        connect(me, &QAtemMixEffect::nextTransitionStyleChanged,
                this, &MainWindow::setTransitionStyle);
        connect(me, &QAtemMixEffect::keyersOnNextTransitionChanged,
                this, &MainWindow::updateKeysOnNextTransition);
        connect(me, &QAtemMixEffect::transitionPreviewChanged,
                this, &MainWindow::updateTransitionPreview);
        connect(m_ui->prevTransBtn, &QPushButton::toggled,
                me, &QAtemMixEffect::setTransitionPreview);
        connect(me, &QAtemMixEffect::transitionPositionChanged,
                this, &MainWindow::setTransitionPosition);
        connect(m_ui->tBar, &QSlider::valueChanged,
                this, &MainWindow::changeTransitionPosition);

        connect(me, &QAtemMixEffect::fadeToBlackFrameCountChanged,
                this, &MainWindow::setFadeToBlackRate);
        connect(me, &QAtemMixEffect::fadeToBlackChanged,
                this, &MainWindow::setFadeToBlack);
        connect(m_ui->ftbBtn, &QPushButton::clicked,
                me, &QAtemMixEffect::toggleFadeToBlack);

        connect(me, &QAtemMixEffect::upstreamKeyOnAirChanged,
                this, &MainWindow::setUpstreamKeyOnAir);

        if(me->programInput() > 0 && me->programInput() <= 8)
        {
            m_programGroup->button(me->programInput())->setChecked(true);
        }
        if(me->previewInput() > 0 && me->previewInput() <= 8)
        {
            m_previewGroup->button(me->previewInput())->setChecked(true);
        }

        setTransitionRate(0, me->transitionFrameCount());
        setTransitionStyle(0, me->nextTransitionStyle());
        updateKeysOnNextTransition(0, me->keyersOnNextTransition());
        m_ui->prevTransBtn->setChecked(me->transitionPreviewEnabled());

        setFadeToBlackRate(0, me->fadeToBlackFrameCount());
        setFadeToBlack(0, me->fadeToBlackFading(), me->fadeToBlackEnabled());

        for(quint8 i = 0; i < me->upstreamKeyCount(); ++i)
        {
            setUpstreamKeyOnAir(0, i, me->upstreamKeyOnAir(i));
        }
    }
    else
    {
        qCritical() << "No M/E found!";
    }
}

void MainWindow::changeProgramInput(int input)
{
    QAtemMixEffect *me = m_atemConnection->mixEffect(0);

    if(me)
    {
        me->changeProgramInput(static_cast<quint16>(input));
    }
    else
    {
        qCritical() << "No M/E found!";
    }
}

void MainWindow::changePreviewInput(int input)
{
    QAtemMixEffect *me = m_atemConnection->mixEffect(0);

    if(me)
    {
        me->changePreviewInput(static_cast<quint16>(input));
    }
    else
    {
        qCritical() << "No M/E found!";
    }
}

void MainWindow::updateProgramInput(quint8 me, quint16 oldInput, quint16 newInput)
{
    if(me == 0)
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
}

void MainWindow::updatePreviewInput(quint8 me, quint16 oldInput, quint16 newInput)
{
    if(me == 0)
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
}

void MainWindow::toogleDsk1Tie()
{
    m_atemConnection->downstreamKey(0)->setTie(!m_atemConnection->downstreamKey(0)->tie());
}

void MainWindow::toogleDsk1OnAir()
{
    m_atemConnection->downstreamKey(0)->setOnAir(!m_atemConnection->downstreamKey(0)->onAir());
}

void MainWindow::doDsk1Auto()
{
    m_atemConnection->downstreamKey(0)->doAuto();
}

void MainWindow::toogleDsk2Tie()
{
    m_atemConnection->downstreamKey(1)->setTie(!m_atemConnection->downstreamKey(1)->tie());
}

void MainWindow::toogleDsk2OnAir()
{
    m_atemConnection->downstreamKey(1)->setOnAir(!m_atemConnection->downstreamKey(1)->onAir());
}

void MainWindow::doDsk2Auto()
{
    m_atemConnection->downstreamKey(1)->doAuto();
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

void MainWindow::setTransitionRate(quint8 me, quint8 rate)
{
    Q_UNUSED(me)

    m_ui->transitionRate->display(rate);
}

void MainWindow::setTransitionStyle(quint8 me, quint8 style)
{
    Q_UNUSED(me)

    m_transitionStyleGroup->button(style)->setChecked(true);
}

void MainWindow::changeTransitionStyle(int style)
{
    if (style != m_atemConnection->mixEffect(0)->nextTransitionStyle())
        m_atemConnection->mixEffect(0)->setTransitionType(static_cast<quint8>(style));
}

void MainWindow::updateKeysOnNextTransition(quint8 me, quint8 keyers)
{
    Q_UNUSED(me)

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
        m_atemConnection->mixEffect(0)->setUpstreamKeyOnNextTransition(static_cast<quint8>(btn - 1), state);
    }
}

void MainWindow::setTransitionPosition(quint8 me, quint16 pos)
{
    Q_UNUSED(me)

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

void MainWindow::setFadeToBlackRate(quint8 me, quint8 rate)
{
    Q_UNUSED(me)

    m_ui->ftbRate->display(rate);
}

void MainWindow::setFadeToBlack(quint8 me, bool fading, bool state)
{
    Q_UNUSED(me)

    m_ui->ftbBtn->setChecked(fading || state);
}

void MainWindow::setUpstreamKeyOnAir(quint8 me, quint8 key, bool state)
{
    Q_UNUSED(me)

    bool block = m_keyOnAirGroup->blockSignals(true);
    m_keyOnAirGroup->button(key)->setChecked(state);
    m_keyOnAirGroup->blockSignals(block);
}

void MainWindow::changeKeyOnAir(int index, bool state)
{
    m_atemConnection->mixEffect(0)->setUpstreamKeyOnAir(index, state);
}

void MainWindow::updateTransitionPreview(quint8 me, bool state)
{
    Q_UNUSED(me)

    m_ui->prevTransBtn->setChecked(state);
}
