#pragma once

#include <experimental/filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "Core.h"

using TrigramCallback = std::function<void(TriGram)>;
using TrigramGenerator = void (*)(const uint8_t *mem, size_t size,
                                  TrigramCallback callback);
namespace fs = std::experimental::filesystem;

std::string_view get_version_string();
std::string random_hex_string(unsigned long length);

TrigramGenerator get_generator_for(IndexType type);
void gen_trigrams(const uint8_t *mem, size_t size, TrigramCallback callback);
void gen_b64grams(const uint8_t *mem, size_t size, TrigramCallback callback);
void gen_wide_b64grams(const uint8_t *mem, size_t size,
                       TrigramCallback callback);
void gen_h4grams(const uint8_t *mem, size_t size, TrigramCallback callback);

void combinations(const QString &qstr, size_t len, const TrigramGenerator &gen,
                  const TrigramCallback &cb);

template <TrigramGenerator gen>
std::vector<TriGram> get_trigrams_eager(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    gen(mem, size, [&](auto val) { out.push_back(val); });

    return out;
}

using TrigramGetter = std::vector<TriGram> (*)(const uint8_t *, size_t);
constexpr TrigramGetter get_trigrams = get_trigrams_eager<gen_trigrams>;
constexpr TrigramGetter get_b64grams = get_trigrams_eager<gen_b64grams>;
constexpr TrigramGetter get_wide_b64grams =
    get_trigrams_eager<gen_wide_b64grams>;
constexpr TrigramGetter get_h4grams = get_trigrams_eager<gen_h4grams>;

/* Represents an object that can be used to writre runs (increasing sequences
of integers - in our case, FileIds. Runs are written in a compressed way -
only differences between consecutive values are saved, and variable length
encoding is used.
*/
class RunWriter {
    std::ostream *out;
    uint64_t out_bytes;
    int64_t prev;

   public:
    /* Create a new clean instance of RunWriter. */
    RunWriter(std::ostream *out) : out(out), out_bytes(0), prev(-1) {}

    /* Write next FileId to the output stream */
    void write(FileId next);

    /* How many bytes was written by this RunWriter object so far */
    uint64_t written_bytes() { return out_bytes; }
};

uint64_t compress_run(const std::vector<FileId> &run, std::ostream &out);
std::vector<FileId> read_compressed_run(const uint8_t *start,
                                        const uint8_t *end);
std::string get_index_type_name(IndexType type);
std::optional<IndexType> index_type_from_string(const std::string &type);

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
                   const std::set<std::string> &index_names,
                   const std::string &fname_list,
                   const std::set<std::string> &taints);

std::string bin_str_to_hex(const std::string &str);
