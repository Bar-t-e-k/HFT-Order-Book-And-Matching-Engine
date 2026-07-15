#pragma once
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <iterator>

template <typename T>
class ObjectPool {
    std::vector<T> pool;

    std::vector<size_t> free_indices;

public:
    explicit ObjectPool(size_t capacity) {
        pool.resize(capacity);
        free_indices.reserve(capacity);

        for (size_t i = capacity; i > 0; --i) {
            free_indices.push_back(i - 1);
        }
    }

    T* acquire() {
        if (free_indices.empty()) [[unlikely]] {
            throw std::out_of_range("Krytyczny błąd: Wyczerpano pulę obiektów ObjectPool!");
        }
        
        size_t index = free_indices.back();
        free_indices.pop_back();
        return &pool[index];
    }

    void release(T* obj) {
        size_t index = std::distance(pool.data(), obj);
        free_indices.push_back(index);
    }

    size_t available() const {
        return free_indices.size();
    }
};