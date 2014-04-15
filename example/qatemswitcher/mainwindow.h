#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QAtemConnection;

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

    void toogleDsk1Tie();
    void toogleDsk1OnAir();
    void doDsk1Auto();
    void toogleDsk2Tie();
    void toogleDsk2OnAir();
    void doDsk2Auto();
    void updateDskTie(quint8 key, bool tie);
    void updateDskOn(quint8 key, bool on);

private:
    Ui::MainWindow *m_ui;
    QAtemConnection *m_atemConnection;
};

#endif // MAINWINDOW_H
