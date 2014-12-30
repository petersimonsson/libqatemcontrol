#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QAtemConnection;
class QButtonGroup;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected slots:
    void connectToAtem();

    void onAtemConnected();

    void changeProgramInput(int input);
    void changePreviewInput(int input);
    void updateProgramInput(quint16 oldInput, quint16 newInput);
    void updatePreviewInput(quint16 oldInput, quint16 newInput);

    void toogleDsk1Tie();
    void toogleDsk1OnAir();
    void doDsk1Auto();
    void toogleDsk2Tie();
    void toogleDsk2OnAir();
    void doDsk2Auto();
    void updateDskTie(quint8 key, bool tie);
    void updateDskOn(quint8 key, bool on);

    void setTransitionRate(quint8 rate);
    void setTransitionStyle(quint8 style);
    void changeTransitionStyle(int style);
    void updateKeysOnNextTransition(quint8 keyers);
    void changeKeysTransition(int btn, bool state);
    void setTransitionPosition(quint16 pos);
    void changeTransitionPosition(int pos);

    void setFadeToBlackRate(quint8 rate);
    void setFadeToBlack(bool fading, bool state);

    void setUpstreamKeyOnAir(quint8 key, bool state);
    void changeKeyOnAir(int index, bool state);

private:
    Ui::MainWindow *m_ui;
    QAtemConnection *m_atemConnection;

    QButtonGroup *m_programGroup;
    QButtonGroup *m_previewGroup;
    QButtonGroup *m_transitionStyleGroup;
    QButtonGroup *m_keysTransitionGroup;
    QButtonGroup *m_keyOnAirGroup;
};

#endif // MAINWINDOW_H
