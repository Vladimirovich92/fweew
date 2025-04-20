#include "bruteforce.h"
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QElapsedTimer>
#include <QDir>
#include <vector>
#include <string>
#include <QtConcurrent>

Bruteforce::Bruteforce(QObject *parent)
    : QObject(parent),
      running(false),
      paused(false),
      gpuInitialized(false),
      processed(0),
      matches(0),
      goodPhrasesCount(0),
      badPhrasesCount(0),
      lastKernelTime(0.0),
      gpuWorkSize(1024),
      bloomFilter(nullptr),
      secp256k1Context(nullptr),
      statsTimer(nullptr),
      statusTimer(nullptr),
      gpuThread(nullptr)
{
    initializeOpenCL();
}

Bruteforce::~Bruteforce()
{
    stop();
    cleanupOpenCL();
    delete bloomFilter;
}

void Bruteforce::setWords(const QStringList &wordList)
{
    words = wordList;
}

void Bruteforce::setAddresses(const QSet<QString> &addressSet)
{
    addresses = addressSet;
}

const QStringList& Bruteforce::getWords() const
{
    return words;
}

const QSet<QString>& Bruteforce::getAddresses() const
{
    return addresses;
}

void Bruteforce::setThreadCount(int count)
{
    threadCount = count;
}

void Bruteforce::setUseGpu(bool use)
{
    useGpu = use;
}

void Bruteforce::setBip39Language(const QString &language)
{
    bip39Language = language;
}

void Bruteforce::setAddressType(const QString &type)
{
    addressType = type;
}

void Bruteforce::updateBloomFilter(int size, double falsePositiveRate)
{
    QElapsedTimer timer;
    timer.start();

    bloom_parameters params;
    params.projected_element_count = size;
    params.false_positive_probability = falsePositiveRate;
    params.compute_optimal_parameters();
    delete bloomFilter;
    bloomFilter = new bloom_filter(params);
    qint64 initTime = timer.elapsed();
    log(tr("Bloom filter initialization: %1 ms").arg(initTime));

    QFuture<void> future = QtConcurrent::run([this]() {
        for (const QString &address : addresses) {
            bloomFilter->insert(address.toStdString());
        }
        qint64 insertTime = QElapsedTimer().elapsed();
        log(tr("Inserted %1 addresses into Bloom filter: %2 ms").arg(addresses.size()).arg(insertTime));
    });
}

void Bruteforce::setGpuWorkSize(size_t size)
{
    gpuWorkSize = size;
    log(tr("Global work size set: %1").arg(size));
}

void Bruteforce::setLogFile(const QString &filePath)
{
    logFilePath = filePath;
}

void Bruteforce::setResultsLogFile(const QString &filePath)
{
    resultsLogFilePath = filePath;
}

void Bruteforce::checkAddress(const QString &address)
{
    bool exists = bloomFilter && bloomFilter->contains(address.toStdString()) && addresses.contains(address);
    emit addressChecked(exists, address);
}

void Bruteforce::initializeOpenCL()
{
    cl_int err;
    std::vector<cl_platform_id> platforms;
    std::vector<cl_device_id> devices;

    cl_uint numPlatforms;
    err = clGetPlatformIDs(0, nullptr, &numPlatforms);
    if (err != CL_SUCCESS || numPlatforms == 0) {
        QString errorMsg = tr("Failed to get OpenCL platforms: error code %1, platforms %2")
                           .arg(err).arg(numPlatforms);
        log(errorMsg);
        emit errorOccurred(errorMsg);
        return;
    }
    log(tr("Found OpenCL platforms: %1").arg(numPlatforms));

    platforms.resize(numPlatforms);
    err = clGetPlatformIDs(numPlatforms, platforms.data(), nullptr);
    if (err != CL_SUCCESS) {
        QString errorMsg = tr("Error getting OpenCL platform info: error code %1").arg(err);
        log(errorMsg);
        emit errorOccurred(errorMsg);
        return;
    }

    bool deviceFound = false;
    for (size_t i = 0; i < platforms.size(); ++i) {
        cl_uint numDevices;
        err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, nullptr, &numDevices);
        if (err == CL_SUCCESS && numDevices > 0) {
            log(tr("Found devices on platform %1: %2").arg(i).arg(numDevices));
            devices.resize(numDevices);
            err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, numDevices, devices.data(), nullptr);
            if (err == CL_SUCCESS) {
                this->platform = platforms[i];
                device = devices[0];
                deviceFound = true;
                break;
            } else {
                log(tr("Error getting devices on platform %1: error code %2").arg(i).arg(err));
            }
        } else {
            log(tr("No devices on platform %1: error code %2").arg(i).arg(err));
        }
    }

    if (!deviceFound) {
        QString errorMsg = tr("No OpenCL devices found.");
        log(errorMsg);
        emit errorOccurred(errorMsg);
        return;
    }

    char deviceName[128];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(deviceName), deviceName, nullptr);
    log(tr("Selected OpenCL device: %1").arg(QString(deviceName)));

    clContext = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        QString errorMsg = tr("Error creating OpenCL context: error code %1").arg(err);
        log(errorMsg);
        emit errorOccurred(errorMsg);
        return;
    }
    log(tr("OpenCL context created successfully."));

    cl_command_queue_properties props[] = { CL_QUEUE_PROPERTIES, 0, 0 };
    queue = clCreateCommandQueueWithProperties(clContext, device, props, &err);
    if (err != CL_SUCCESS) {
        QString errorMsg = tr("Error creating OpenCL command queue: error code %1").arg(err);
        log(errorMsg);
        emit errorOccurred(errorMsg);
        clReleaseContext(clContext);
        clContext = nullptr;
        return;
    }
    log(tr("OpenCL command queue created successfully."));

    log(tr("OpenCL initialized successfully."));
    gpuInitialized = true;
}

void Bruteforce::configureGpuWorkSizes()
{
    gpuWorkSize = 1024;
    log(tr("Set GPU work sizes: global=%1").arg(gpuWorkSize));
}

void Bruteforce::loadOpenCLKernel()
{
    if (!gpuInitialized) {
        log(tr("OpenCL not initialized."));
        emit errorOccurred(tr("OpenCL not initialized."));
        return;
    }

    QFile kernelFile(":/kernel.cl");
    if (!kernelFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log(tr("Failed to open kernel.cl file."));
        emit errorOccurred(tr("Failed to open kernel file."));
        gpuInitialized = false;
        return;
    }

    QTextStream in(&kernelFile);
    QString kernelSource = in.readAll();
    kernelFile.close();

    const char *source = kernelSource.toUtf8().constData();
    size_t sourceSize = strlen(source);
    cl_int err;
    program = clCreateProgramWithSource(clContext, 1, &source, &sourceSize, &err);
    if (err != CL_SUCCESS) {
        log(tr("Error creating OpenCL program: error code %1").arg(err));
        emit errorOccurred(tr("Error creating OpenCL program: %1").arg(err));
        gpuInitialized = false;
        return;
    }

    const char *options = "-cl-std=CL1.2 -Werror";
    err = clBuildProgram(program, 1, &device, options, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        size_t logSize;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
        std::vector<char> buildLog(logSize);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, buildLog.data(), nullptr);
        QString buildLogStr(buildLog.data());
        log(tr("Error compiling kernel: %1\nBuild log: %2").arg(err).arg(buildLogStr));
        emit errorOccurred(tr("Error compiling kernel: %1\nLog: %2").arg(err).arg(buildLogStr));
        clReleaseProgram(program);
        program = nullptr;
        gpuInitialized = false;
        return;
    }

    kernel = clCreateKernel(program, "generate_address", &err);
    if (err != CL_SUCCESS) {
        log(tr("Error creating kernel: error code %1").arg(err));
        emit errorOccurred(tr("Error creating kernel: %1").arg(err));
        clReleaseProgram(program);
        program = nullptr;
        gpuInitialized = false;
        return;
    }

    size_t wordsSize = words.size() * WORD_SIZE * sizeof(char);
    log(tr("Creating words buffer: size=%1 bytes").arg(wordsSize));
    wordsBuffer = clCreateBuffer(clContext, CL_MEM_READ_ONLY, wordsSize, nullptr, &err);
    if (err != CL_SUCCESS) {
        log(tr("Error creating words buffer: error code %1").arg(err));
        emit errorOccurred(tr("Error creating words buffer: %1").arg(err));
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        wordsBuffer = nullptr;
        kernel = nullptr;
        program = nullptr;
        gpuInitialized = false;
        return;
    }

    size_t outputSize = gpuWorkSize * sizeof(int);
    log(tr("Creating output buffer: size=%1 bytes").arg(outputSize));
    outputBuffer = clCreateBuffer(clContext, CL_MEM_WRITE_ONLY, outputSize, nullptr, &err);
    if (err != CL_SUCCESS) {
        log(tr("Error creating output buffer: error code %1").arg(err));
        emit errorOccurred(tr("Error creating output buffer: %1").arg(err));
        clReleaseMemObject(wordsBuffer);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        wordsBuffer = nullptr;
        kernel = nullptr;
        program = nullptr;
        gpuInitialized = false;
        return;
    }

    size_t seedSize = gpuWorkSize * BIP39_SEED_BYTES;
    log(tr("Creating seed buffer: size=%1 bytes").arg(seedSize));
    seedBuffer = clCreateBuffer(clContext, CL_MEM_WRITE_ONLY, seedSize, nullptr, &err);
    if (err != CL_SUCCESS) {
        log(tr("Error creating seed buffer: error code %1").arg(err));
        emit errorOccurred(tr("Error creating seed buffer: %1").arg(err));
        clReleaseMemObject(wordsBuffer);
        clReleaseMemObject(outputBuffer);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        wordsBuffer = nullptr;
        outputBuffer = nullptr;
        kernel = nullptr;
        program = nullptr;
        gpuInitialized = false;
        return;
    }

    size_t addressSize = gpuWorkSize * ADDRESS_SIZE * sizeof(char);
    log(tr("Creating addresses buffer: size=%1 bytes").arg(addressSize));
    addressBuffer = clCreateBuffer(clContext, CL_MEM_WRITE_ONLY, addressSize, nullptr, &err);
    if (err != CL_SUCCESS) {
        log(tr("Error creating addresses buffer: error code %1").arg(err));
        emit errorOccurred(tr("Error creating addresses buffer: %1").arg(err));
        clReleaseMemObject(wordsBuffer);
        clReleaseMemObject(outputBuffer);
        clReleaseMemObject(seedBuffer);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        wordsBuffer = nullptr;
        outputBuffer = nullptr;
        seedBuffer = nullptr;
        kernel = nullptr;
        program = nullptr;
        gpuInitialized = false;
        return;
    }

    std::vector<char> wordData(wordsSize, 0);
    size_t offset = 0;
    for (const QString &word : words) {
        QByteArray wordBytes = word.toUtf8();
        size_t len = std::min<size_t>(wordBytes.size(), WORD_SIZE - 1);
        memcpy(wordData.data() + offset, wordBytes.constData(), len);
        offset += WORD_SIZE;
    }

    err = clEnqueueWriteBuffer(queue, wordsBuffer, CL_TRUE, 0, wordsSize, wordData.data(), 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        log(tr("Error writing dictionary to buffer: error code %1").arg(err));
        emit errorOccurred(tr("Error writing dictionary to buffer: %1").arg(err));
        cleanupOpenCL();
        gpuInitialized = false;
        return;
    }

    log(tr("OpenCL kernel loaded successfully."));
}

void Bruteforce::generatePhrasesGpu()
{
    if (!gpuInitialized || !kernel || !queue) {
        log(tr("Error: OpenCL not initialized or kernel not loaded."));
        emit errorOccurred(tr("OpenCL not initialized."));
        stop();
        return;
    }

    cl_int err;
    while (running && !paused) {
        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &wordsBuffer);
        int wordCount = words.size();
        err |= clSetKernelArg(kernel, 1, sizeof(int), &wordCount);
        err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &seedBuffer);
        err |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &addressBuffer);
        err |= clSetKernelArg(kernel, 4, sizeof(cl_mem), &outputBuffer);
        if (err != CL_SUCCESS) {
            log(tr("Error setting kernel arguments: error code %1").arg(err));
            emit errorOccurred(tr("Error setting kernel arguments: %1").arg(err));
            stop();
            return;
        }

        cl_event event;
        QElapsedTimer kernelTimer;
        kernelTimer.start();
        err = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &gpuWorkSize, nullptr, 0, nullptr, &event);
        if (err != CL_SUCCESS) {
            log(tr("Error launching kernel: error code %1").arg(err));
            emit errorOccurred(tr("Error launching kernel: %1").arg(err));
            stop();
            return;
        }

        clWaitForEvents(1, &event);
        lastKernelTime = kernelTimer.nsecsElapsed() / 1e9;

        std::vector<char> addressesData(gpuWorkSize * ADDRESS_SIZE);
        err = clEnqueueReadBuffer(queue, addressBuffer, CL_TRUE, 0, gpuWorkSize * ADDRESS_SIZE, addressesData.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            log(tr("Error reading addresses buffer: error code %1").arg(err));
            emit errorOccurred(tr("Error reading addresses buffer: %1").arg(err));
            stop();
            return;
        }

        std::vector<int> results(gpuWorkSize);
        err = clEnqueueReadBuffer(queue, outputBuffer, CL_TRUE, 0, gpuWorkSize * sizeof(int), results.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            log(tr("Error reading output buffer: error code %1").arg(err));
            emit errorOccurred(tr("Error reading output buffer: %1").arg(err));
            stop();
            return;
        }

        for (size_t i = 0; i < gpuWorkSize && running && !paused; ++i) {
            if (results[i]) {
                QString address(addressesData.data() + i * ADDRESS_SIZE);
                if (bloomFilter && bloomFilter->contains(address.toStdString()) && addresses.contains(address)) {
                    matches++;
                    emit matchFound(address, "Unknown Phrase", "Unknown WIF");
                    logResult(tr("Match found: %1").arg(address));
                }
                processed++;
                goodPhrasesCount++;
            } else {
                badPhrasesCount++;
            }
        }

        QThread::msleep(10);
    }
}

void Bruteforce::start()
{
    if (!running) {
        running = true;
        paused = false;
        if (useGpu && gpuInitialized) {
            log(tr("Starting in GPU mode."));
            generatePhrasesGpu();
        } else {
            log(tr("Starting in CPU mode (GPU unavailable or disabled)."));
            generatePhrasesCpu();
        }
    }
}

void Bruteforce::pause()
{
    paused = true;
}

void Bruteforce::resume()
{
    paused = false;
}

void Bruteforce::stop()
{
    running = false;
    paused = false;
}

void Bruteforce::cleanupOpenCL()
{
    if (wordsBuffer) {
        clReleaseMemObject(wordsBuffer);
        wordsBuffer = nullptr;
    }
    if (outputBuffer) {
        clReleaseMemObject(outputBuffer);
        outputBuffer = nullptr;
    }
    if (seedBuffer) {
        clReleaseMemObject(seedBuffer);
        seedBuffer = nullptr;
    }
    if (addressBuffer) {
        clReleaseMemObject(addressBuffer);
        addressBuffer = nullptr;
    }
    if (kernel) {
        clReleaseKernel(kernel);
        kernel = nullptr;
    }
    if (program) {
        clReleaseProgram(program);
        program = nullptr;
    }
    if (queue) {
        clReleaseCommandQueue(queue);
        queue = nullptr;
    }
    if (clContext) {
        clReleaseContext(clContext);
        clContext = nullptr;
    }

    gpuInitialized = false;
}

void Bruteforce::log(const QString &message)
{
    emit logMessage(message);
}

void Bruteforce::logResult(const QString &message)
{
    emit logMessage(message);
}

int Bruteforce::progress() const
{
    return 0;
}

qint64 Bruteforce::processedCount() const
{
    return processed;
}

qint64 Bruteforce::matchesFound() const
{
    return matches;
}

double Bruteforce::speed() const
{
    return 0.0;
}

qint64 Bruteforce::badPhrases() const
{
    return badPhrasesCount;
}

qint64 Bruteforce::goodPhrases() const
{
    return goodPhrasesCount;
}

bool Bruteforce::isRunning() const
{
    return running;
}

bool Bruteforce::isPaused() const
{
    return paused;
}

bool Bruteforce::isGpuAvailable() const
{
    return gpuInitialized;
}

void Bruteforce::initializeSecp256k1() {}
bool Bruteforce::checkAvailableMemory(size_t) { return true; }
QStringList Bruteforce::generatePhrase() { return QStringList(); }
QByteArray Bruteforce::phraseToSeed(const QStringList &, const QString &) { return QByteArray(); }
QString Bruteforce::seedToPrivateKey(const QByteArray &) { return QString(); }
QString Bruteforce::privateKeyToAddress(const QString &) { return QString(); }
QString Bruteforce::privateKeyToWIF(const QString &) { return QString(); }
bool Bruteforce::verifyChecksum(const QString &) { return true; }
void Bruteforce::generatePhrasesCpu() {}
void Bruteforce::processBatchCpu(const QVector<QStringList> &) {}
void Bruteforce::logError(const QString &error) { log(error); }
QString Bruteforce::base58Encode(const QByteArray &) { return QString(); }
QByteArray Bruteforce::base58Decode(const QString &) { return QByteArray(); }
QString Bruteforce::bech32Encode(const QByteArray &) { return QString(); }
QByteArray Bruteforce::bech32Decode(const QString &) { return QByteArray(); }
QString Bruteforce::toHex(const QByteArray &) { return QString(); }
QByteArray Bruteforce::fromHex(const QString &) { return QByteArray(); }
void Bruteforce::updateSystemStatus() {}
qint64 Bruteforce::getCurrentMemoryUsage() { return 0; }