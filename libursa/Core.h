#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

using FileId = uint32_t;
using TriGram = uint32_t;

constexpr uint32_t NUM_TRIGRAMS = 16777216;
constexpr uint32_t DB_MAGIC = 0xCA7DA7A;
constexpr size_t DEFAULT_MAX_MEM_SIZE = 1024L * 1024L * 1024L * 2L;  // 2 GB
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

enum class BuilderType { FLAT = 1, BITMAP = 2 };

enum class QTokenType {
    CHAR = 1,       // normal byte e.g. \xAA
    WILDCARD = 2,   // full wildcard \x??
    HWILDCARD = 3,  // high wildcard e.g. \x?A
    LWILDCARD = 4   // low wildcard e.g. \xA?
};

class QToken {
    QTokenType type_;
    uint8_t val_;

   public:
    QToken(QTokenType type) : type_(type), val_(0) {}
    QToken(QTokenType type, uint8_t val) : type_(type), val_(val) {}

    QTokenType type() const { return type_; }
    uint8_t val() const { return val_; }
    bool is_wildcard() const {
        return type() == QTokenType::WILDCARD ||
               type() == QTokenType::HWILDCARD ||
               type() == QTokenType::LWILDCARD;
    }

    std::vector<uint8_t> possible_values() const;

    bool operator==(const QToken &a) const {
        return type_ == a.type_ && val_ == a.val_;
    }
};

using QString = std::vector<QToken>;
