#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "Core.h"

using TrigramGenerator = std::vector<TriGram> (*)(const uint8_t *mem, size_t size);

TrigramGenerator get_generator_for(IndexType type);
std::vector<TriGram> get_trigrams(const uint8_t *mem, size_t size);
std::vector<TriGram> get_b64grams(const uint8_t *mem, size_t size);
std::vector<TriGram> get_h4grams(const uint8_t *mem, size_t size);
void compress_run(const std::vector<FileId> &run, std::ostream &out);
std::vector<FileId> read_compressed_run(const uint8_t *start, const uint8_t *end);
std::string get_index_type_name(IndexType type);
constexpr int get_b64_value(uint8_t character) {
    constexpr int ALPHABET_SIZE = 'Z' - 'A' + 1;
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    } else if (character >= 'a' && character <= 'z') {
        return character - 'a' + ALPHABET_SIZE;
    } else if (character >= '0' && character <= '9') {
        return character - '0' + (2 * ALPHABET_SIZE);
    } else if (character == ' ') {
        return 2 * ALPHABET_SIZE + 10;
    } else if (character == '\n') {
        return 2 * ALPHABET_SIZE + 10 + 1;
    } else {
        return -1;
    }
}
