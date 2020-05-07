#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using FileId = uint32_t;
using TriGram = uint32_t;

constexpr uint32_t NUM_TRIGRAMS = 16777216;
constexpr uint32_t DB_MAGIC = 0xCA7DA7A;
constexpr size_t INDEXER_COMPACT_THRESHOLD = 20;

enum class IndexType : uint32_t {
    // Trigrams. "abcdefgh" -> "abc", "bcd", "cde", "def", "efg"
    GRAM3 = 1,
    // Text-4grams. Charset "[a-zA-Z0-9 \n]". "abcde" -> b64("abcd"),
    // b64("bcde")
    TEXT4 = 2,
    // Hashed 4grams. "abcdef" -> H("abcd"), H("bcde"), H("cdef")
    HASH4 = 3,
    // Utf-16 4grams. "a\0b\0c\0d\0e\0" -> b64("abcd"), b64("bcde")
    WIDE8 = 4
};

constexpr bool is_valid_index_type(uint32_t type) {
    // Be careful here. This looks complex, but avoids undefined behaviour.
    switch (static_cast<IndexType>(type)) {
        case IndexType::GRAM3:
            [[fallthrough]];
        case IndexType::TEXT4:
            [[fallthrough]];
        case IndexType::HASH4:
            [[fallthrough]];
        case IndexType::WIDE8:
            return true;
    }
    return false;
}

std::string get_index_type_name(IndexType type);

std::optional<IndexType> index_type_from_string(const std::string &type);

enum class BuilderType { FLAT = 1, BITMAP = 2 };
