#pragma once

#include <string>
#include <vector>
#include <set>
#include <functional>
#include <experimental/filesystem>

#include "Core.h"

using TrigramCallback = std::function<void (TriGram)>;
using TrigramGenerator = void (*)(const uint8_t *mem, size_t size, TrigramCallback callback);
namespace fs = std::experimental::filesystem;

TrigramGenerator get_generator_for(IndexType type);
void gen_trigrams(const uint8_t *mem, size_t size, TrigramCallback callback);
void gen_b64grams(const uint8_t *mem, size_t size, TrigramCallback callback);
void gen_wide_b64grams(const uint8_t *mem, size_t size, TrigramCallback callback);
void gen_h4grams(const uint8_t *mem, size_t size, TrigramCallback callback);

template <TrigramGenerator gen>
std::vector<TriGram> get_trigrams_eager(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    gen(mem, size, [&](auto val) {
        out.push_back(val);
    });

    return out;
}

using TrigramGetter = std::vector<TriGram>(*)(const uint8_t *, size_t);
constexpr TrigramGetter get_trigrams = get_trigrams_eager<gen_trigrams>;
constexpr TrigramGetter get_b64grams = get_trigrams_eager<gen_b64grams>;
constexpr TrigramGetter get_wide_b64grams = get_trigrams_eager<gen_wide_b64grams>;
constexpr TrigramGetter get_h4grams = get_trigrams_eager<gen_h4grams>;

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

void store_dataset(const fs::path &db_base, const std::string &fname,
                   const std::set<std::string> &index_names, const std::vector<std::string> &fids);
