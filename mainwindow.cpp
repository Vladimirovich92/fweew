#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QProgressDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      bruteforce(new Bruteforce(this))
{
    ui->setupUi(this);
    connect(bruteforce, &Bruteforce::logMessage, this, &MainWindow::logMessage);
    connect(bruteforce, &Bruteforce::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(bruteforce, &Bruteforce::matchFound, this, &MainWindow::onMatchFound);
    connect(bruteforce, &Bruteforce::addressChecked, this, &MainWindow::onAddressChecked);
    connect(ui->useGpuCheckBox, &QCheckBox::toggled, this, &MainWindow::onUseGpuToggled);

    ui->startButton->setEnabled(false);
    ui->pauseButton->setEnabled(false);
    ui->stopButton->setEnabled(false);
    ui->useGpuCheckBox->setEnabled(bruteforce->isGpuAvailable());
    ui->useGpuCheckBox->setChecked(bruteforce->isGpuAvailable());

    ui->gpuWorkSizeComboBox->setCurrentText("1024");
    ui->gpuWorkSizeComboBox->setEnabled(ui->useGpuCheckBox->isChecked());

    ui->bloomFilterSizeSpinBox->setValue(1000000);
    ui->bloomFalsePositiveSpinBox->setValue(0.1);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::logMessage(const QString &message)
{
    ui->logTextEdit->append(message);
}

void MainWindow::onErrorOccurred(const QString &error)
{
    QMessageBox::critical(this, tr("Error"), error);
}

void MainWindow::onMatchFound(const QString &address, const QString &phrase, const QString &wif)
{
    QString message = tr("Match found!\nAddress: %1\nPhrase: %2\nWIF: %3").arg(address, phrase, wif);
    ui->logTextEdit->append(message);
}

void MainWindow::onAddressChecked(bool exists, const QString &address)
{
    if (exists) {
        ui->logTextEdit->append(tr("Address exists: %1").arg(address));
    }
}

bool MainWindow::validateAddress(const QString &address)
{
    if (address.isEmpty()) return false;
    if (address.startsWith("1") && address.length() >= 25 && address.length() <= 34) {
        return address.contains(QRegularExpression("^[a-km-zA-HJ-NP-Z1-9]+$"));
    }
    if (address.startsWith("bc1") && address.length() >= 39 && address.length() <= 59) {
        return address.contains(QRegularExpression("^[ac-hj-np-z02-9]+$"));
    }
    return false;
}

void MainWindow::on_loadWordsButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open words file"), "", tr("Text files (*.txt)"));
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QStringList words;
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString word = in.readLine().trimmed();
                if (!word.isEmpty()) {
                    words.append(word);
                }
            }
            file.close();
            bruteforce->setWords(words);
            ui->startButton->setEnabled(!words.isEmpty() && !bruteforce->getAddresses().isEmpty());
            logMessage(tr("Loaded words: %1").arg(words.size()));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to open words file."));
        }
    }
}

void MainWindow::on_loadAddressesButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open addresses file"), "", tr("Text files (*.txt)"));
    if (!fileName.isEmpty()) {
        ui->loadAddressesButton->setEnabled(false);
        QProgressDialog progress(tr("Loading addresses..."), tr("Cancel"), 0, 100, this);
        progress.setWindowModality(Qt::WindowModal);

        QFuture<void> future = QtConcurrent::run([this, fileName, &progress]() {
            QElapsedTimer timer;
            timer.start();

            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qint64 readTime = timer.elapsed();
                logMessage(tr("File reading: %1 ms").arg(readTime));

                QSet<QString> addresses;
                QByteArray data = file.readAll();
                QStringList lines = QString(data).split('\n', Qt::SkipEmptyParts);
                int totalLines = lines.size();
                for (int i = 0; i < totalLines; ++i) {
                    QString &address = lines[i];
                    address = address.trimmed();
                    if (!address.isEmpty() && validateAddress(address)) {
                        addresses.insert(address);
                    }
                    if (i % 1000 == 0) {
                        QMetaObject::invokeMethod(&progress, "setValue", Q_ARG(int, (i * 100) / totalLines));
                    }
                }
                qint64 processTime = timer.elapsed();
                logMessage(tr("Address processing: %1 ms").arg(processTime - readTime));

                file.close();
                bruteforce->setAddresses(addresses);
                qint64 setTime = timer.elapsed();
                logMessage(tr("Address setting: %1 ms").arg(setTime - processTime));

                QMetaObject::invokeMethod(ui->startButton, "setEnabled",
                                          Q_ARG(bool, !bruteforce->getWords().isEmpty() && !addresses.isEmpty()));
                QMetaObject::invokeMethod(this, "logMessage",
                                          Q_ARG(QString, tr("Loaded addresses: %1, total time: %2 ms")
                                                .arg(addresses.size()).arg(timer.elapsed())));
                QMetaObject::invokeMethod(ui->loadAddressesButton, "setEnabled", Q_ARG(bool, true));
                QMetaObject::invokeMethod(&progress, "setValue", Q_ARG(int, 100));
            } else {
                QMetaObject::invokeMethod(this, [this]() {
                    QMessageBox::warning(this, tr("Error"), tr("Failed to open addresses file."));
                    ui->loadAddressesButton->setEnabled(true);
                });
            }
        });
    }
}

void MainWindow::on_startButton_clicked()
{
    bruteforce->setThreadCount(ui->threadsSpinBox->value());
    bruteforce->setUseGpu(ui->useGpuCheckBox->isChecked());
    bruteforce->setGpuWorkSize(ui->gpuWorkSizeComboBox->currentText().toULongLong());
    bruteforce->updateBloomFilter(ui->bloomFilterSizeSpinBox->value(),
                                  ui->bloomFalsePositiveSpinBox->value() / 100.0);
    bruteforce->start();
    ui->startButton->setEnabled(false);
    ui->pauseButton->setEnabled(true);
    ui->stopButton->setEnabled(true);
}

void MainWindow::on_pauseButton_clicked()
{
    bruteforce->pause();
    ui->pauseButton->setEnabled(false);
}

void MainWindow::on_stopButton_clicked()
{
    bruteforce->stop();
    ui->startButton->setEnabled(!bruteforce->getWords().isEmpty() && !bruteforce->getAddresses().isEmpty());
    ui->pauseButton->setEnabled(false);
    ui->stopButton->setEnabled(false);
}

void MainWindow::on_recommendBloomSettings_clicked()
{
    int addressCount = bruteforce->getAddresses().size();
    if (addressCount == 0) {
        QMessageBox::warning(this, tr("Error"), tr("Load addresses first."));
        return;
    }
    bloom_parameters params;
    params.projected_element_count = addressCount;
    params.false_positive_probability = 0.001;
    params.compute_optimal_parameters();
    ui->bloomFilterSizeSpinBox->setValue(params.projected_element_count);
    ui->bloomFalsePositiveSpinBox->setValue(params.false_positive_probability * 100);
}

void MainWindow::onUseGpuToggled(bool checked)
{
    bruteforce->setUseGpu(checked);
    ui->gpuWorkSizeComboBox->setEnabled(checked);
}