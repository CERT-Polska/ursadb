#pragma once

#include <experimental/filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "Core.h"
#include "QString.h"

using TrigramCallback = std::function<void(TriGram)>;

// Trigram generator - will call callback for every availbale trigram.
using TrigramGenerator = void (*)(const uint8_t *mem, uint64_t size,
                                  const TrigramCallback &callback);

// Validator for the token value. First parameter is offset, second token char.
using TokenValidator = std::function<bool(uint32_t, uint8_t)>;

// TODO get rid of this define
namespace fs = std::experimental::filesystem;

std::string_view get_version_string();
std::string random_hex_string(uint64_t length);

// Returns a function that can be used to generate ngrams of the specified type.
TrigramGenerator get_generator_for(IndexType type);

// Returns a TokenValidator for ngrams of the specified type.
TokenValidator get_validator_for(IndexType type);

// Returns a number of bytes needed for ngram of the specified type.
size_t get_ngram_size_for(IndexType type);

void gen_trigrams(const uint8_t *mem, uint64_t size, const TrigramCallback &cb);
void gen_b64grams(const uint8_t *mem, uint64_t size, const TrigramCallback &cb);
void gen_wide_b64grams(const uint8_t *mem, uint64_t size,
                       const TrigramCallback &cb);
void gen_h4grams(const uint8_t *mem, uint64_t size, const TrigramCallback &cb);

void combinations(const QString &qstr, size_t len, const TrigramGenerator &gen,
                  const TrigramCallback &cb);

// Converts ngram from raw representation to compressed 3byte id.
std::optional<TriGram> convert_gram(IndexType type, uint64_t source);
std::optional<TriGram> convert_gram(IndexType type, int index, const QString &string);

template <TrigramGenerator gen>
std::vector<TriGram> get_trigrams_eager(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    gen(mem, size, [&](auto val) { out.push_back(val); });

    return out;
}

uint64_t get_milli_timestamp();

using TrigramGetter = std::vector<TriGram> (*)(const uint8_t *, size_t);
constexpr TrigramGetter get_trigrams = get_trigrams_eager<gen_trigrams>;
constexpr TrigramGetter get_b64grams = get_trigrams_eager<gen_b64grams>;
constexpr TrigramGetter get_wide_b64grams =
    get_trigrams_eager<gen_wide_b64grams>;
constexpr TrigramGetter get_h4grams = get_trigrams_eager<gen_h4grams>;

// Represents an object that can be used to write runs (increasing sequences
// of integers - in our case, FileIds. Runs are written in a compressed way -
// only differences between consecutive values are saved, and variable length
// encoding is used.
class RunWriter {
    std::ostream *out;
    uint64_t out_bytes;
    int64_t prev;

   public:
    // Create a new clean instance of RunWriter.
    RunWriter(std::ostream *out) : out(out), out_bytes(0), prev(-1) {}

    // Write next FileId to the output stream
    void write(FileId next);

    // How many bytes was written by this RunWriter object so far.
    uint64_t written_bytes() { return out_bytes; }
};

// Equivalent to RunWriter, but uses file descriptors instead of ostream.
// Also buffers intermediate results explicitly.
// Eventually we'll migrate everything to this class.
class PosixRunWriter {
    int fd_;
    uint64_t out_bytes_;
    int64_t prev_;
    std::vector<uint8_t> buffer_;

   public:
    // Creates a new clean instance of RunWriter.
    PosixRunWriter(int fd) : fd_(fd), out_bytes_(0), prev_(-1), buffer_() {}

    PosixRunWriter(const PosixRunWriter &other) = delete;
    PosixRunWriter(PosixRunWriter &&other) = default;
    ~PosixRunWriter();

    // Writes next FileId to the output stream.
    void write(FileId next);

    // Writes run of FileIds without decompressing them.
    void write_raw(FileId base, uint8_t *start, const uint8_t *end);

    // Flush buffered changes to the backing fd.
    void flush();

    // How many bytes were written by this RunWriter object so far
    uint64_t bytes_written() const { return out_bytes_; }

    void reset() {
        out_bytes_ = 0;
        prev_ = -1;
    }
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
                   std::optional<std::string_view> fname_cache,
                   const std::set<std::string> &taints);

std::string bin_str_to_hex(const std::string &str);
