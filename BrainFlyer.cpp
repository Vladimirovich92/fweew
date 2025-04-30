#include "BrainFlyer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGradient>
#include <QMessageBox>
#include <sstream>
#include <iomanip>
#include <functional>
#include <QTimer>
#include <thread>
#include <fstream>
#include <QFileDialog>
#include <QStringConverter>
#include <chrono>
#include <set>

namespace BrainFlyer {

BrainFlyer::BrainFlyer(QWidget *parent) : QMainWindow(parent), bloomFilter(30000000, 0.0001) {
    secp_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    setupUI();
    initOpenCL();
    logFile.setFileName("brainflyer.log");
    matchFile.setFileName("matches.txt");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append))
        logSize = logFile.size();
    if (matchFile.open(QIODevice::WriteOnly | QIODevice::Append))
        matchSize = matchFile.size();
}

BrainFlyer::~BrainFlyer() {
    stopProcessing();
    secp256k1_context_destroy(secp_ctx);
    if (kernel) clReleaseKernel(kernel);
    if (program) clReleaseProgram(program);
    if (queue) clReleaseCommandQueue(queue);
    if (context) clReleaseContext(context);
    if (device) clReleaseDevice(device);
    logFile.close();
    matchFile.close();
}

void BrainFlyer::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, QColor(44, 62, 80));
    gradient.setColorAt(1, QColor(52, 152, 219));
    QPalette palette;
    palette.setBrush(QPalette::Window, gradient);
    centralWidget->setAutoFillBackground(true);
    centralWidget->setPalette(palette);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    loadAddrBtn = new QPushButton("Load Address List", this);
    loadPassBtn = new QPushButton("Load Passphrase List", this);
    startBtn = new QPushButton("Start", this);
    stopBtn = new QPushButton("Stop", this);
    stopBtn->setEnabled(false);

    buttonLayout->addWidget(loadAddrBtn);
    buttonLayout->addWidget(loadPassBtn);
    buttonLayout->addWidget(startBtn);
    buttonLayout->addWidget(stopBtn);

    progressBar = new QProgressBar(this);
    statusLabel = new QLabel("Status: Idle", this);
    speedLabel = new QLabel("Speed: 0 keys/s", this);
    processedLabel = new QLabel("Processed: 0", this);
    passphraseCountLabel = new QLabel("Loaded Passphrases: 0", this);
    logText = new QTextEdit(this);
    logText->setReadOnly(true);

    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(speedLabel);
    mainLayout->addWidget(processedLabel);
    mainLayout->addWidget(passphraseCountLabel);
    mainLayout->addWidget(logText);

    connect(loadAddrBtn, &QPushButton::clicked, this, &BrainFlyer::loadAddressList);
    connect(loadPassBtn, &QPushButton::clicked, this, &BrainFlyer::loadPassphraseList);
    connect(startBtn, &QPushButton::clicked, this, &BrainFlyer::startProcessing);
    connect(stopBtn, &QPushButton::clicked, this, &BrainFlyer::stopProcessing);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        speedLabel->setText(QString("Speed: %1 keys/s").arg(processedCount / 5));
        processedLabel->setText(QString("Processed: %1").arg(processedCount.load()));
        processedCount = 0;
    });
    timer->start(5000);
}

void BrainFlyer::initOpenCL() {
    cl_int err;
    cl_uint num_platforms;
    err = clGetPlatformIDs(1, &platform, &num_platforms);
    if (err != CL_SUCCESS) {
        logMessage(QString("Failed to get OpenCL platform: Error code %1").arg(err));
        return;
    }
    logMessage(QString("Found %1 OpenCL platform(s)").arg(num_platforms));

    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
    if (err != CL_SUCCESS) {
        logMessage(QString("Failed to get OpenCL device: Error code %1").arg(err));
        return;
    }

    char deviceName[1024];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(deviceName), deviceName, nullptr);
    logMessage(QString("Using device: %1").arg(deviceName));

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        logMessage(QString("Failed to create OpenCL context: Error code %1").arg(err));
        return;
    }

    cl_queue_properties props[] = {0};
    queue = clCreateCommandQueueWithProperties(context, device, props, &err);
    if (err != CL_SUCCESS) {
        logMessage(QString("Failed to create OpenCL command queue: Error code %1").arg(err));
        return;
    }

    std::ifstream kernelFile("kernel.cl");
    if (!kernelFile.is_open()) {
        logMessage("Failed to open kernel.cl file");
        return;
    }
    std::string kernelCode((std::istreambuf_iterator<char>(kernelFile)), std::istreambuf_iterator<char>());
    const char *code = kernelCode.c_str();
    size_t code_length = kernelCode.length();

    program = clCreateProgramWithSource(context, 1, &code, &code_length, &err);
    if (err != CL_SUCCESS) {
        logMessage(QString("Failed to create OpenCL program: Error code %1").arg(err));
        return;
    }

    err = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        char build_log[4096];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(build_log), build_log, nullptr);
        logMessage(QString("Failed to build OpenCL program: %1").arg(build_log));
        return;
    }

    kernel = clCreateKernel(program, "generateAddresses", &err);
    if (err != CL_SUCCESS) {
        logMessage(QString("Failed to create OpenCL kernel: Error code %1").arg(err));
        return;
    }
}

void BrainFlyer::loadAddressList() {
    QString filePath = QFileDialog::getOpenFileName(this, "Load Address List", "", "Text Files (*.txt)");
    if (filePath.isEmpty()) return;

    std::ifstream file(filePath.toStdString());
    if (!file.is_open()) {
        logMessage("Failed to open address file");
        return;
    }

    std::string address;
    while (std::getline(file, address)) {
        if (!address.empty()) {
            bloomFilter.add(address);
            addressCount++;
        }
    }
    file.close();
    logMessage(QString("Loaded %1 addresses from %2").arg(addressCount).arg(filePath));
}

void BrainFlyer::loadPassphraseList() {
    QString filePath = QFileDialog::getOpenFileName(this, "Load Passphrase List", "", "Text Files (*.txt)");
    if (filePath.isEmpty()) return;

    passphraseFilePath = filePath.toStdString();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logMessage("Failed to open passphrase file");
        return;
    }

    QTextStream in(&file);
    QByteArray firstBytes = file.peek(4);
    if (firstBytes.startsWith(QByteArray("\xEF\xBB\xBF"))) {
        in.setEncoding(QStringConverter::Utf8);
    } else if (firstBytes.startsWith(QByteArray("\xFF\xFE")) || firstBytes.startsWith(QByteArray("\xFE\xFF"))) {
        in.setEncoding(QStringConverter::Utf16);
    } else {
        in.setEncoding(QStringConverter::Utf8);
    }

    passphrases.clear();
    passphraseCount = 0;
    while (!in.atEnd()) {
        QString passphrase = in.readLine().trimmed();
        if (!passphrase.isEmpty()) {
            passphrases.push_back(passphrase);
            passphraseCount++;
        }
    }
    file.close();
    passphraseCountLabel->setText(QString("Loaded Passphrases: %1").arg(passphraseCount));
    logMessage(QString("Loaded %1 passphrases from %2").arg(passphraseCount).arg(filePath));
}

void BrainFlyer::startProcessing() {
    if (passphrases.empty()) {
        logMessage("No passphrase file loaded");
        return;
    }

    running = true;
    startBtn->setEnabled(false);
    stopBtn->setEnabled(true);
    statusLabel->setText("Status: Running");
    processedCount = 0;

    resultProcessingThread = std::thread([this]() {
        processResults();
    });

    std::thread([this]() {
        processPassphrases();
        running = false;
        resultsQueueCondition.notify_all();
        resultProcessingThread.join();
        QMetaObject::invokeMethod(this, [this]() {
            startBtn->setEnabled(true);
            stopBtn->setEnabled(false);
            statusLabel->setText("Status: Idle");
        }, Qt::QueuedConnection);
    }).detach();
}

void BrainFlyer::stopProcessing() {
    running = false;
    resultsQueueCondition.notify_all();
}

void BrainFlyer::logMessage(const QString &message) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp).arg(message);

    if (logFile.isOpen()) {
        QTextStream out(&logFile);
        out << logEntry << "\n";
        logSize += logEntry.size() + 1;
        if (logSize > maxLogSize) {
            logFile.close();
            logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
            logSize = 0;
        }
    }
    logText->append(logEntry);
}

void BrainFlyer::verifyPublicKey(const std::string& priv_key_hex, const QString& passphrase) {
    if (priv_key_hex.size() != 64) {
        logMessage(QString("Invalid private key length for %1: %2").arg(passphrase).arg(priv_key_hex.size()));
        return;
    }

    std::vector<unsigned char> priv_key(32);
    try {
        for (size_t i = 0; i < 32; ++i) {
            std::string byte_str = priv_key_hex.substr(i * 2, 2);
            if (!std::all_of(byte_str.begin(), byte_str.end(), ::isxdigit)) {
                logMessage(QString("Invalid hex character in private key for %1: %2").arg(passphrase).arg(QString::fromStdString(byte_str)));
                return;
            }
            priv_key[i] = std::stoi(byte_str, nullptr, 16);
        }
    } catch (const std::exception& e) {
        logMessage(QString("Failed to parse private key for passphrase %1: %2").arg(passphrase).arg(e.what()));
        return;
    }

    logMessage(QString("Parsed private key for %1: %2").arg(passphrase).arg(QByteArray((char*)priv_key.data(), 32).toHex()));

    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(secp_ctx, &pubkey, priv_key.data())) {
        logMessage(QString("Failed to create public key for passphrase: %1").arg(passphrase));
        return;
    }

    unsigned char pubkey_comp[33], pubkey_uncomp[65];
    size_t comp_len = 33, uncomp_len = 65;
    secp256k1_ec_pubkey_serialize(secp_ctx, pubkey_comp, &comp_len, &pubkey, SECP256K1_EC_COMPRESSED);
    secp256k1_ec_pubkey_serialize(secp_ctx, pubkey_uncomp, &uncomp_len, &pubkey, SECP256K1_EC_UNCOMPRESSED);

    QString pub_x = QByteArray((char*)(pubkey_uncomp + 1), 32).toHex();
    QString pub_y = QByteArray((char*)(pubkey_uncomp + 33), 32).toHex();
    logMessage(QString("secp256k1 PubKey for %1 (Comp): %2").arg(passphrase).arg(QByteArray((char*)pubkey_comp, comp_len).toHex()));
    logMessage(QString("secp256k1 PubKey for %1 (Uncomp): %2").arg(passphrase).arg(QByteArray((char*)pubkey_uncomp, uncomp_len).toHex()));
    logMessage(QString("secp256k1 Pub X for %1: %2").arg(passphrase).arg(pub_x));
    logMessage(QString("secp256k1 Pub Y for %1: %2").arg(passphrase).arg(pub_y));
}

void BrainFlyer::processResults() {
    std::set<size_t> processedBatches;
    while (running || !resultsQueue.empty()) {
        std::unique_lock<std::mutex> lock(resultsQueueMutex);
        resultsQueueCondition.wait(lock, [this]() { return !resultsQueue.empty() || !running; });

        if (!resultsQueue.empty()) {
            ResultData data = std::move(resultsQueue.front());
            resultsQueue.pop();
            lock.unlock();

            if (processedBatches.find(data.batchStartIdx) != processedBatches.end()) {
                logMessage(QString("Skipping duplicate batch at index %1").arg(data.batchStartIdx));
                continue;
            }
            processedBatches.insert(data.batchStartIdx);

            auto processStartTime = std::chrono::high_resolution_clock::now();
            logMessage(QString("Processing results for batch starting at index %1, size %2").arg(data.batchStartIdx).arg(data.batchSize));

            size_t expectedSize = data.batchSize * (34 + 34 + 64 + 52);
            logMessage(QString("OutputData size: %1, expected: %2").arg(data.outputData.size()).arg(expectedSize));
            if (data.outputData.empty()) {
                logMessage(QString("Error: Output data for batch starting at index %1 is empty").arg(data.batchStartIdx));
                continue;
            }
            if (data.outputData.size() != expectedSize) {
                logMessage(QString("Error: Output data size mismatch for batch %1").arg(data.batchStartIdx));
                continue;
            }

            QString debugBuffer;
            size_t debugLen = std::min<size_t>(100, data.outputData.size());
            for (size_t j = 0; j < debugLen; ++j) {
                debugBuffer += QString("%1 ").arg(data.outputData[j], 2, 16, QChar('0'));
            }
            logMessage(QString("Debug: First %1 bytes of outputData for batch %2: %3").arg(debugLen).arg(data.batchStartIdx).arg(debugBuffer));

            for (size_t i = 0; i < data.batchSize; ++i) {
                logMessage(QString("Processing phrase %1 in batch %2").arg(i).arg(data.batchStartIdx));
                size_t offset = i * (34 + 34 + 64 + 52);
                std::vector<unsigned char>& output = data.outputData;

                if (offset + (34 + 34 + 64 + 52) > output.size()) {
                    logMessage(QString("Error: Invalid offset %1 for batch starting at index %2, output size %3")
                               .arg(offset).arg(data.batchStartIdx).arg(output.size()));
                    continue;
                }

                QString raw_buffer;
                for (size_t j = offset; j < offset + (34 + 34 + 64 + 52); ++j) {
                    raw_buffer += QString("%1 ").arg(output[j], 2, 16, QChar('0'));
                }
                QString passphrase = (data.batchStartIdx + i < passphrases.size()) ? passphrases[data.batchStartIdx + i] : "Unknown";
                logMessage(QString("Raw buffer for %1: %2").arg(passphrase).arg(raw_buffer));

                std::string p2pkh_comp, p2pkh_uncomp, priv_key, wif;
                size_t pos = offset;
                for (size_t j = 0; j < 34 && pos < offset + 34 && pos < output.size(); ++j) {
                    if (output[pos] == 0) break;
                    p2pkh_comp += static_cast<char>(output[pos++]);
                }
                pos = offset + 34;
                for (size_t j = 0; j < 34 && pos < offset + 68 && pos < output.size(); ++j) {
                    if (output[pos] == 0) break;
                    p2pkh_uncomp += static_cast<char>(output[pos++]);
                }
                pos = offset + 68;
                for (size_t j = 0; j < 64 && pos < offset + 132 && pos < output.size(); ++j) {
                    priv_key += static_cast<char>(output[pos++]);
                }
                pos = offset + 132;
                for (size_t j = 0; j < 52 && pos < offset + 184 && pos < output.size(); ++j) {
                    if (output[pos] == 0) break;
                    wif += static_cast<char>(output[pos++]);
                }

                if (p2pkh_comp.empty() && p2pkh_uncomp.empty() && priv_key.empty() && wif.empty()) {
                    logMessage(QString("Warning: Empty data for passphrase %1 at offset %2").arg(passphrase).arg(offset));
                    continue;
                }

                if (priv_key.size() != 64) {
                    logMessage(QString("Error: Invalid private key length %1 for passphrase %2").arg(priv_key.size()).arg(passphrase));
                    continue;
                }
                if (wif.size() > 52) {
                    logMessage(QString("Error: Invalid WIF length %1 for passphrase %2").arg(wif.size()).arg(passphrase));
                    continue;
                }
                if (p2pkh_comp.size() > 34) {
                    logMessage(QString("Error: Invalid P2PKH (Compressed) length %1 for passphrase %2").arg(p2pkh_comp.size()).arg(passphrase));
                    continue;
                }
                if (p2pkh_uncomp.size() > 34) {
                    logMessage(QString("Error: Invalid P2PKH (Uncompressed) length %1 for passphrase %2").arg(p2pkh_uncomp.size()).arg(passphrase));
                    continue;
                }

                QString logEntry = QString("Passphrase: %1\nP2PKH (Compressed): %2\nP2PKH (Uncompressed): %3\nPrivate Key: %4\nWIF: %5")
                                      .arg(passphrase)
                                      .arg(QString::fromStdString(p2pkh_comp))
                                      .arg(QString::fromStdString(p2pkh_uncomp))
                                      .arg(QString::fromStdString(priv_key))
                                      .arg(QString::fromStdString(wif));
                logMessage(logEntry);

                verifyPublicKey(priv_key, passphrase);

                if (!p2pkh_comp.empty() && !p2pkh_uncomp.empty()) {
                    if (bloomFilter.contains(p2pkh_comp) || bloomFilter.contains(p2pkh_uncomp)) {
                        QString matchEntry = QString("Match found for passphrase: %1\nP2PKH (Compressed): %2\nP2PKH (Uncompressed): %3\nPrivate Key: %4\nWIF: %5")
                                                .arg(passphrase)
                                                .arg(QString::fromStdString(p2pkh_comp))
                                                .arg(QString::fromStdString(p2pkh_uncomp))
                                                .arg(QString::fromStdString(priv_key))
                                                .arg(QString::fromStdString(wif));
                        if (matchFile.isOpen()) {
                            QTextStream out(&matchFile);
                            out << matchEntry << "\n\n";
                            matchSize += matchEntry.size() + 2;
                            if (matchSize > maxLogSize) {
                                matchFile.close();
                                matchFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
                                matchSize = 0;
                            }
                        }
                        logMessage(matchEntry);
                    }
                }

                processedCount++;
            }
            auto processEndTime = std::chrono::high_resolution_clock::now();
            auto processDuration = std::chrono::duration_cast<std::chrono::milliseconds>(processEndTime - processStartTime).count();
            logMessage(QString("Batch starting at %1: Processing results took %2 ms").arg(data.batchStartIdx / 2000 + 1).arg(processDuration));
        }
    }
}

void BrainFlyer::processPassphrases() {
    cl_int err;
    size_t processed = 0;

    const size_t batchSize = 1000; // Fixed: Reduced batch size to 1000 to reduce VRAM load
    size_t totalPhrases = passphrases.size();
    size_t numBatches = (totalPhrases + batchSize - 1) / batchSize;

    std::vector<unsigned char> passphraseBytes[3];
    std::vector<unsigned char> outputData[3];
    cl_mem passphraseBuffers[3] = {nullptr, nullptr, nullptr};
    cl_mem outputBuffers[3] = {nullptr, nullptr, nullptr};
    size_t outputSizePerBatch = batchSize * (34 + 34 + 64 + 52);
    cl_event kernelEvents[3] = {nullptr, nullptr, nullptr};
    cl_event readEvents[3] = {nullptr, nullptr, nullptr};

    size_t currentBatch = 0;
    int bufferIndex = 0;

    auto batchStartTime = std::chrono::high_resolution_clock::now();

    // Fixed: Query preferred work group size for RX 580
    size_t localWorkSize;
    err = clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &localWorkSize, nullptr);
    if (err != CL_SUCCESS) {
        logMessage(QString("Failed to get preferred work group size: Error code %1").arg(err));
        localWorkSize = 64; // Fallback to 64 for RX 580
    } else {
        logMessage(QString("Preferred local work size: %1").arg(localWorkSize));
    }

    while (currentBatch < numBatches && running) {
        size_t startIdx = currentBatch * batchSize;
        size_t currentBatchSize = std::min(batchSize, totalPhrases - startIdx);

        batchStartTime = std::chrono::high_resolution_clock::now();
        logMessage(QString("Processing batch %1 of %2 (phrases %3 to %4)")
                   .arg(currentBatch + 1)
                   .arg(numBatches)
                   .arg(startIdx + 1)
                   .arg(startIdx + currentBatchSize));

        passphraseBytes[bufferIndex].clear();
        for (size_t i = startIdx; i < startIdx + currentBatchSize; ++i) {
            const QString& passphrase = passphrases[i];
            QByteArray passphraseUtf8 = passphrase.toUtf8();
            size_t len = std::min<size_t>(passphraseUtf8.size(), 100);
            for (size_t j = 0; j < len; ++j) {
                passphraseBytes[bufferIndex].push_back(static_cast<unsigned char>(passphraseUtf8[j]));
            }
            passphraseBytes[bufferIndex].push_back(0);
            while ((passphraseBytes[bufferIndex].size() % 101) != 0) {
                passphraseBytes[bufferIndex].push_back(0);
            }
        }

        logMessage(QString("Prepared input buffer for batch %1, size %2 bytes").arg(currentBatch + 1).arg(passphraseBytes[bufferIndex].size()));

        // Fixed: Wait for previous buffer to be free
        if (passphraseBuffers[bufferIndex] != nullptr) {
            err = clWaitForEvents(1, &readEvents[bufferIndex]);
            if (err != CL_SUCCESS) {
                logMessage(QString("Failed to wait for previous read event for buffer %1: Error code %2").arg(bufferIndex).arg(err));
                return;
            }
            clReleaseMemObject(passphraseBuffers[bufferIndex]);
            clReleaseMemObject(outputBuffers[bufferIndex]);
            clReleaseEvent(kernelEvents[bufferIndex]);
            clReleaseEvent(readEvents[bufferIndex]);
            passphraseBuffers[bufferIndex] = nullptr;
            outputBuffers[bufferIndex] = nullptr;
            kernelEvents[bufferIndex] = nullptr;
            readEvents[bufferIndex] = nullptr;
        }

        passphraseBuffers[bufferIndex] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                        passphraseBytes[bufferIndex].size(), passphraseBytes[bufferIndex].data(), &err);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to create input buffer for batch %1: Error code %2").arg(currentBatch + 1).arg(err));
            return;
        }

        outputBuffers[bufferIndex] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, outputSizePerBatch, nullptr, &err);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to create output buffer for batch %1: Error code %2").arg(currentBatch + 1).arg(err));
            clReleaseMemObject(passphraseBuffers[bufferIndex]);
            return;
        }

        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &outputBuffers[bufferIndex]);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to set output buffer arg for batch %1: Error code %2").arg(currentBatch + 1).arg(err));
            clReleaseMemObject(passphraseBuffers[bufferIndex]);
            clReleaseMemObject(outputBuffers[bufferIndex]);
            return;
        }
        err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &passphraseBuffers[bufferIndex]);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to set input buffer arg for batch %1: Error code %2").arg(currentBatch + 1).arg(err));
            clReleaseMemObject(passphraseBuffers[bufferIndex]);
            clReleaseMemObject(outputBuffers[bufferIndex]);
            return;
        }
        err = clSetKernelArg(kernel, 2, sizeof(cl_ulong), &currentBatchSize);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to set batch size arg for batch %1: Error code %2").arg(currentBatch + 1).arg(err));
            clReleaseMemObject(passphraseBuffers[bufferIndex]);
            clReleaseMemObject(outputBuffers[bufferIndex]);
            return;
        }

        // Fixed: Use local work size for better RX 580 performance
        size_t global_work_size = currentBatchSize;
        if (global_work_size % localWorkSize != 0) {
            global_work_size = ((global_work_size / localWorkSize) + 1) * localWorkSize;
        }
        err = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_work_size, &localWorkSize, 0, nullptr, &kernelEvents[bufferIndex]);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to enqueue kernel for batch %1: Error code %2").arg(currentBatch + 1).arg(err));
            clReleaseMemObject(passphraseBuffers[bufferIndex]);
            clReleaseMemObject(outputBuffers[bufferIndex]);
            return;
        }

        outputData[bufferIndex].resize(outputSizePerBatch);
        err = clEnqueueReadBuffer(queue, outputBuffers[bufferIndex], CL_FALSE, 0, outputSizePerBatch,
                                  outputData[bufferIndex].data(), 1, &kernelEvents[bufferIndex], &readEvents[bufferIndex]);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to enqueue read buffer for batch %1: Error code %2").arg(currentBatch + 1).arg(err));
            clReleaseMemObject(passphraseBuffers[bufferIndex]);
            clReleaseMemObject(outputBuffers[bufferIndex]);
            clReleaseEvent(kernelEvents[bufferIndex]);
            return;
        }

        logMessage(QString("Enqueued kernel and read buffer for batch %1").arg(currentBatch + 1));

        // Fixed: Process previous batch only after its read is complete
        if (currentBatch >= 1) {
            int prevBufferIndex = (bufferIndex + 2) % 3; // Process buffer from previous iteration
            err = clWaitForEvents(1, &readEvents[prevBufferIndex]);
            if (err != CL_SUCCESS) {
                logMessage(QString("Failed to wait for read event for batch %1: Error code %2").arg(currentBatch).arg(err));
                clReleaseMemObject(passphraseBuffers[prevBufferIndex]);
                clReleaseMemObject(outputBuffers[prevBufferIndex]);
                clReleaseEvent(kernelEvents[prevBufferIndex]);
                clReleaseEvent(readEvents[prevBufferIndex]);
                return;
            }

            auto readEndTime = std::chrono::high_resolution_clock::now();
            auto readDuration = std::chrono::duration_cast<std::chrono::milliseconds>(readEndTime - batchStartTime).count();
            logMessage(QString("Batch %1: Kernel + Read buffer took %2 ms").arg(currentBatch).arg(readDuration));

            ResultData resultData;
            resultData.outputData = std::move(outputData[prevBufferIndex]);
            resultData.batchSize = std::min(batchSize, totalPhrases - ((currentBatch - 1) * batchSize));
            resultData.batchStartIdx = (currentBatch - 1) * batchSize;
            {
                std::lock_guard<std::mutex> lock(resultsQueueMutex);
                resultsQueue.push(std::move(resultData));
            }
            resultsQueueCondition.notify_one();

            clReleaseMemObject(passphraseBuffers[prevBufferIndex]);
            clReleaseMemObject(outputBuffers[prevBufferIndex]);
            clReleaseEvent(kernelEvents[prevBufferIndex]);
            clReleaseEvent(readEvents[prevBufferIndex]);
            passphraseBuffers[prevBufferIndex] = nullptr;
            outputBuffers[prevBufferIndex] = nullptr;
            kernelEvents[prevBufferIndex] = nullptr;
            readEvents[prevBufferIndex] = nullptr;
        }

        bufferIndex = (bufferIndex + 1) % 3;
        processed += currentBatchSize;
        currentBatch++;

        auto batchEndTime = std::chrono::high_resolution_clock::now();
        auto batchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(batchEndTime - batchStartTime).count();
        logMessage(QString("Batch %1: Total execution time %2 ms").arg(currentBatch).arg(batchDuration));

        // Fixed: Ensure queue is flushed to avoid resource accumulation
        err = clFinish(queue);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to finish queue for batch %1: Error code %2").arg(currentBatch).arg(err));
            return;
        }
    }

    // Fixed: Process remaining batches with proper synchronization
    for (size_t remaining = (currentBatch > 1 ? std::min<size_t>(2, currentBatch) : currentBatch); remaining > 0 && running; --remaining) {
        int prevBufferIndex = (bufferIndex + (3 - remaining)) % 3;
        err = clWaitForEvents(1, &readEvents[prevBufferIndex]);
        if (err != CL_SUCCESS) {
            logMessage(QString("Failed to wait for read event for remaining batch %1: Error code %2").arg(currentBatch - remaining + 1).arg(err));
            clReleaseMemObject(passphraseBuffers[prevBufferIndex]);
            clReleaseMemObject(outputBuffers[prevBufferIndex]);
            clReleaseEvent(kernelEvents[prevBufferIndex]);
            clReleaseEvent(readEvents[prevBufferIndex]);
            continue;
        }

        auto readEndTime = std::chrono::high_resolution_clock::now();
        auto readDuration = std::chrono::duration_cast<std::chrono::milliseconds>(readEndTime - batchStartTime).count();
        logMessage(QString("Remaining Batch %1: Kernel + Read buffer took %2 ms").arg(currentBatch - remaining + 1).arg(readDuration));

        ResultData resultData;
        resultData.outputData = std::move(outputData[prevBufferIndex]);
        resultData.batchSize = std::min(batchSize, totalPhrases - ((currentBatch - remaining) * batchSize));
        resultData.batchStartIdx = (currentBatch - remaining) * batchSize;
        {
            std::lock_guard<std::mutex> lock(resultsQueueMutex);
            resultsQueue.push(std::move(resultData));
        }
        resultsQueueCondition.notify_one();

        clReleaseMemObject(passphraseBuffers[prevBufferIndex]);
        clReleaseMemObject(outputBuffers[prevBufferIndex]);
        clReleaseEvent(kernelEvents[prevBufferIndex]);
        clReleaseEvent(readEvents[prevBufferIndex]);
        passphraseBuffers[prevBufferIndex] = nullptr;
        outputBuffers[prevBufferIndex] = nullptr;
        kernelEvents[prevBufferIndex] = nullptr;
        readEvents[prevBufferIndex] = nullptr;
    }

    logMessage(QString("Total processed passphrases: %1").arg(processed));
}

} // namespace BrainFlyer