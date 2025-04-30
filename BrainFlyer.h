#ifndef BRAINFLYER_H
#define BRAINFLYER_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QFile>
#include <CL/cl.h>
#include <secp256k1.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread> // Добавлено для std::thread
#include <atomic>
#include <vector>
#include <cstdint>
#include <string>

namespace BrainFlyer {

class BloomFilter {
public:
    BloomFilter(uint64_t size, double falsePositiveRate);
    void add(const std::string& item);
    bool contains(const std::string& item) const;

private:
    uint64_t m_size;
    std::vector<bool> m_bits;
    uint32_t m_numHashes;
    uint32_t hash(const std::string& item, uint32_t seed) const;
};

class BrainFlyer : public QMainWindow {
    Q_OBJECT

public:
    explicit BrainFlyer(QWidget *parent = nullptr);
    ~BrainFlyer();

private:
    void setupUI();
    void initOpenCL();
    void loadAddressList();
    void loadPassphraseList();
    void startProcessing();
    void stopProcessing();
    void logMessage(const QString &message);
    void verifyPublicKey(const std::string& priv_key_hex, const QString& passphrase);
    std::string base58_encode(const unsigned char *input, size_t len);
    void SHA256(const unsigned char *data, size_t len, unsigned char *hash);
    void RIPEMD160(const unsigned char *data, size_t len, unsigned char *hash);
    void processResults();
    void processPassphrases();

    QPushButton *loadAddrBtn;
    QPushButton *loadPassBtn;
    QPushButton *startBtn;
    QPushButton *stopBtn;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QLabel *speedLabel;
    QLabel *processedLabel;
    QLabel *passphraseCountLabel;
    QTextEdit *logText;

    BloomFilter bloomFilter;
    std::vector<QString> passphrases;
    std::string passphraseFilePath;
    size_t addressCount = 0;
    size_t passphraseCount = 0;

    QFile logFile;
    QFile matchFile;
    size_t logSize = 0;
    size_t matchSize = 0;
    const size_t maxLogSize = 10 * 1024 * 1024;

    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;

    secp256k1_context *secp_ctx;

    std::atomic<bool> running{false};
    std::atomic<size_t> processedCount{0};
    std::thread resultProcessingThread;

    struct ResultData {
        std::vector<unsigned char> outputData;
        size_t batchSize;
        size_t batchStartIdx;
    };

    std::queue<ResultData> resultsQueue;
    std::mutex resultsQueueMutex;
    std::condition_variable resultsQueueCondition;

    // Temporary storage for P2PKH addresses
    std::string p2pkh_comp;
    std::string p2pkh_uncomp;
};

} // namespace BrainFlyer

#endif // BRAINFLYER_H