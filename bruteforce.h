#ifndef BRUTEFORCE_H
#define BRUTEFORCE_H

#include <QObject>
#include <QStringList>
#include <QSet>
#include <QMutex>
#include <QTimer>
#include <QThread>
#include <QFile>
#include <QIODevice>
#include <QThreadPool>
#include <bloom_filter.hpp>
#include <secp256k1.h>
#include <CL/cl.h>
#include <QElapsedTimer>

class Bruteforce : public QObject
{
    Q_OBJECT

public:
    explicit Bruteforce(QObject *parent = nullptr);
    ~Bruteforce();

    void setWords(const QStringList &w);
    void setAddresses(const QSet<QString> &a);
    const QStringList& getWords() const;
    const QSet<QString>& getAddresses() const;
    void setThreadCount(int count);
    void setUseGpu(bool use);
    void setBip39Language(const QString &language);
    void setAddressType(const QString &type);
    void updateBloomFilter(int size, double falsePositiveRate);
    void setGpuWorkSize(size_t size);
    void setLogFile(const QString &filePath);
    void setResultsLogFile(const QString &filePath);
    void checkAddress(const QString &address);

    void start();
    void stop();
    void pause();
    void resume();
    int progress() const;
    qint64 processedCount() const;
    qint64 matchesFound() const;
    double speed() const;
    qint64 badPhrases() const;
    qint64 goodPhrases() const;
    bool isRunning() const;
    bool isPaused() const;
    bool isGpuAvailable() const;

    static constexpr qint64 MAX_ADDRESSES = 20000000;
    static constexpr qint64 LOG_SIZE_LIMIT = 200 * 1024 * 1024;
    static constexpr qint64 RESULTS_LOG_SIZE_LIMIT = 100 * 1024 * 1024;

signals:
    void matchFound(const QString &address, const QString &phrase, const QString &wif);
    void addressChecked(bool exists, const QString &address);
    void errorOccurred(const QString &error);
    void logMessage(const QString &message);
    void statusUpdated(const QString &cpuUsage, const QString &gpuUsage, const QString &gpuTemp, const QString &gpuMemory, double kernelTime);
    void memoryUsageUpdated(qint64 usedMemory, qint64 totalMemory);

private:
    void initializeSecp256k1();
    bool checkAvailableMemory(size_t requiredMemory);
    void initializeOpenCL();
    void configureGpuWorkSizes();
    void loadOpenCLKernel();
    void cleanupOpenCL();
    QStringList generatePhrase();
    QByteArray phraseToSeed(const QStringList &phrase, const QString &passphrase = "");
    QString seedToPrivateKey(const QByteArray &seed);
    QString privateKeyToAddress(const QString &privateKey);
    QString privateKeyToWIF(const QString &privateKey);
    bool verifyChecksum(const QString &address);
    void generatePhrasesCpu();
    void processBatchCpu(const QVector<QStringList> &phrases);
    void log(const QString &message);
    void logResult(const QString &message);
    void logError(const QString &error);
    QString base58Encode(const QByteArray &data);
    QByteArray base58Decode(const QString &data);
    QString bech32Encode(const QByteArray &data);
    QByteArray bech32Decode(const QString &address);
    QString toHex(const QByteArray &data);
    QByteArray fromHex(const QString &hex);
    void updateSystemStatus();
    qint64 getCurrentMemoryUsage();

    bool running;
    bool paused;
    bool gpuInitialized;
    qint64 processed;
    qint64 matches;
    qint64 goodPhrasesCount;
    qint64 badPhrasesCount;
    double lastKernelTime;
    size_t gpuWorkSize;
    bloom_filter *bloomFilter;
    secp256k1_context *secp256k1Context;
    QTimer *statsTimer;
    QTimer *statusTimer;
    QThread *gpuThread;
    QStringList words;
    QSet<QString> addresses;
    int threadCount;
    bool useGpu;
    QMutex mutex;
    QElapsedTimer speedTimer;
    qint64 lastProcessed;
    QString bip39Language;
    QString addressType;
    QString logFilePath;
    QString resultsLogFilePath;
    cl_platform_id platform;
    cl_device_id device;
    cl_context clContext;
    cl_program program;
    cl_command_queue queue;
    cl_kernel kernel;
    cl_mem wordsBuffer;
    cl_mem outputBuffer;
    cl_mem seedBuffer;
    cl_mem addressBuffer;
    QThreadPool threadPool;

    static constexpr int PHRASE_WORDS = 12;
    static constexpr int WORD_SIZE = 256;
    static constexpr int BIP39_SEED_BYTES = 64;
    static constexpr int ADDRESS_SIZE = 34;
};

#endif // BRUTEFORCE_H