#ifndef BLOOM_FILTER_HPP
#define BLOOM_FILTER_HPP

#include <vector>
#include <string>
#include <cmath>
#include <functional>
#include <cstdint>

// Параметры Bloom-фильтра
struct bloom_parameters {
    unsigned long projected_element_count; // Ожидаемое количество элементов
    double false_positive_probability;    // Допустимая вероятность ложного срабатывания
    unsigned long number_of_hashes;       // Количество хэш-функций
    unsigned long table_size;             // Размер битового массива

    bloom_parameters()
        : projected_element_count(0),
          false_positive_probability(0.0),
          number_of_hashes(0),
          table_size(0) {}

    void compute_optimal_parameters() {
        if (projected_element_count == 0 || false_positive_probability <= 0.0 || false_positive_probability >= 1.0) {
            return;
        }

        // Формула для оптимального размера таблицы: m = -n * ln(p) / (ln(2)^2)
        double ln_2 = 0.69314718056;
        table_size = static_cast<unsigned long>(
            -static_cast<double>(projected_element_count) * log(false_positive_probability) / (ln_2 * ln_2)
        );

        // Формула для оптимального количества хэш-функций: k = (m/n) * ln(2)
        number_of_hashes = static_cast<unsigned long>(
            (static_cast<double>(table_size) / projected_element_count) * ln_2
        );

        if (number_of_hashes < 1) number_of_hashes = 1;
        if (table_size < 1) table_size = 1;
    }

    // Рекомендация параметров на основе количества адресов
    static bloom_parameters recommend_parameters(unsigned long address_count) {
        bloom_parameters params;
        params.projected_element_count = address_count;
        params.false_positive_probability = 0.001; // По умолчанию 0.1%
        params.compute_optimal_parameters();
        return params;
    }
};

// Реализация Bloom-фильтра
class bloom_filter {
public:
    bloom_filter() : bit_table_(0), hash_count_(0) {}

    explicit bloom_filter(const bloom_parameters& params)
        : bit_table_(params.table_size, false), hash_count_(params.number_of_hashes) {}

    void insert(const std::string& key) {
        for (unsigned long i = 0; i < hash_count_; ++i) {
            bit_table_[compute_hash(key, i) % bit_table_.size()] = true;
        }
    }

    bool contains(const std::string& key) const {
        for (unsigned long i = 0; i < hash_count_; ++i) {
            if (!bit_table_[compute_hash(key, i) % bit_table_.size()]) {
                return false;
            }
        }
        return true;
    }

    void clear() {
        std::fill(bit_table_.begin(), bit_table_.end(), false);
    }

private:
    std::vector<bool> bit_table_;    // Битовый массив
    unsigned long hash_count_;       // Количество хэш-функций

    // Простая хэш-функция (используем метод FNV-1a с разными семенами)
    uint64_t compute_hash(const std::string& key, unsigned long seed) const {
        const uint64_t FNV_PRIME = 1099511628211ULL;
        const uint64_t FNV_OFFSET = 14695981039346656037ULL;

        uint64_t hash = FNV_OFFSET ^ seed;
        for (char c : key) {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }
};

#endif // BLOOM_FILTER_HPP