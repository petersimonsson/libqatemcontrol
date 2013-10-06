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

private:
    Ui::MainWindow *m_ui;
    QAtemConnection *m_atemConnection;
};

#endif // MAINWINDOW_H
