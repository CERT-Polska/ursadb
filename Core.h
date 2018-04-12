#pragma once

#include <stdint.h>

using FileId = uint32_t;
using TriGram = uint32_t;

constexpr uint32_t NUM_TRIGRAMS = 16777216;
constexpr uint32_t DB_MAGIC = 0xCA7DA7A;

enum class IndexType {
    GRAM3 = 1
};
