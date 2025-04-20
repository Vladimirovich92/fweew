#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "bruteforce.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void logMessage(const QString &message);
    void onErrorOccurred(const QString &error);
    void onMatchFound(const QString &address, const QString &phrase, const QString &wif);
    void onAddressChecked(bool exists, const QString &address);
    void onUseGpuToggled(bool checked);

private slots:
    void on_loadWordsButton_clicked();
    void on_loadAddressesButton_clicked();
    void on_startButton_clicked();
    void on_pauseButton_clicked();
    void on_stopButton_clicked();
    void on_recommendBloomSettings_clicked();

private:
    bool validateAddress(const QString &address);

    Ui::MainWindow *ui;
    Bruteforce *bruteforce;
};

#endif // MAINWINDOW_H