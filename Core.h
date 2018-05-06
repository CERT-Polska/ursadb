#pragma once

#include <cstddef>
#include <cstdint>

using FileId = uint32_t;
using TriGram = uint32_t;

constexpr uint32_t NUM_TRIGRAMS = 16777216;
constexpr uint32_t DB_MAGIC = 0xCA7DA7A;
constexpr size_t DEFAULT_MAX_MEM_SIZE = 1024L * 1024L * 1024L * 2L; // 2 GB
constexpr size_t MAX_INDEXER_TEMP_DATASETS = 20;

enum class IndexType {
    // Trigrams. "abcdefgh" -> "abc", "bcd", "cde", "def", "efg"
    GRAM3 = 1,
    // Text-4grams. Charset "[a-zA-Z0-9 \n]". "abcde" -> b64("abcd"), b64("bcde")
    TEXT4 = 2,
    // Hashed 4grams. "abcdef" -> H("abcd"), H("bcde"), H("cdef")
    HASH4 = 3,
    // Utf-16 4grams. "a\0b\0c\0d\0e\0" -> b64("abcd"), b64("bcde")
    WIDE8 = 4
};

constexpr bool is_valid_index_type(uint32_t type) {
    // Be very careful here. This looks complex, but avoids undevined behaviour.
    switch (type) {
        case static_cast<uint32_t>(IndexType::GRAM3):
        case static_cast<uint32_t>(IndexType::TEXT4):
        case static_cast<uint32_t>(IndexType::HASH4):
        case static_cast<uint32_t>(IndexType::WIDE8):
            return true;
    }
    return false;
}
