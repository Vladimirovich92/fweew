#include "BrainFlyer.h"
#include <cstdint>
#include <vector>
#include <functional>
#include <cmath>
#include <string>

namespace BrainFlyer {

BloomFilter::BloomFilter(uint64_t size, double falsePositiveRate) : m_size(size) {
    m_bits.resize(size, false);
    // Используем falsePositiveRate для вычисления оптимального числа хэш-функций
    m_numHashes = static_cast<uint32_t>(std::ceil(-(std::log(falsePositiveRate) / std::log(2)) * std::log(2)));
    if (m_numHashes == 0) m_numHashes = 1;
}

void BloomFilter::add(const std::string& item) {
    for (uint32_t i = 0; i < m_numHashes; ++i) {
        uint64_t index = hash(item, i) % m_size;
        m_bits[index] = true;
    }
}

bool BloomFilter::contains(const std::string& item) const {
    for (uint32_t i = 0; i < m_numHashes; ++i) {
        uint64_t index = hash(item, i) % m_size;
        if (!m_bits[index]) return false;
    }
    return true;
}

uint32_t BloomFilter::hash(const std::string& item, uint32_t seed) const {
    std::hash<std::string> hasher;
    return static_cast<uint32_t>(hasher(item + std::to_string(seed)));
}

} // namespace BrainFlyer