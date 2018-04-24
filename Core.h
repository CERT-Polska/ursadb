#pragma once

#include <cstddef>
#include <cstdint>

using FileId = uint32_t;
using TriGram = uint32_t;

constexpr uint32_t NUM_TRIGRAMS = 16777216;
constexpr uint32_t DB_MAGIC = 0xCA7DA7A;
constexpr size_t DEFAULT_MAX_MEM_SIZE = 1024L * 1024L * 1024L * 2L; // 2 GB

enum class IndexType {
    GRAM3 = 1,
    TEXT4 = 2,
    HASH4 = 3,
    WIDE4 = 4
};

constexpr bool is_valid_index_type(uint32_t type) {
    // Be very careful here. This looks complex, but avoids undevined behaviour.
    switch (type) {
        case static_cast<uint32_t>(IndexType::GRAM3):
        case static_cast<uint32_t>(IndexType::TEXT4):
        case static_cast<uint32_t>(IndexType::HASH4):
        case static_cast<uint32_t>(IndexType::WIDE4):
            return true;
    }
    return false;
}