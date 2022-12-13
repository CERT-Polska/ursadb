#include "Utils.h"

#include <unistd.h>

#include <fstream>
#include <iomanip>
#include <random>
#include <set>

#include "Json.h"
#include "Version.h"
#include "spdlog/spdlog.h"

std::string_view get_version_string() { return ursadb_version_string; }

std::string random_hex_string(uint64_t length) {
    constexpr static char charset[] = "0123456789abcdef";
    thread_local static std::random_device rd;
    thread_local static std::seed_seq seed{
        rd(), rd(), rd(), rd()};  // A bit better than pathetic default
    thread_local static std::mt19937_64 random(seed);
    thread_local static std::uniform_int_distribution<int> pick(
        0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);

    for (uint64_t i = 0; i < length; i++) {
        result += charset[pick(random)];
    }

    return result;
}

uint64_t get_milli_timestamp() {
    namespace c = std::chrono;
    auto timestamp = c::steady_clock::now().time_since_epoch();
    return c::duration_cast<c::milliseconds>(timestamp).count();
}

size_t get_ngram_size_for(IndexType type) {
    switch (type) {
        case IndexType::GRAM3:
            return 3;
        case IndexType::HASH4:
            return 4;
        case IndexType::TEXT4:
            return 4;
        case IndexType::WIDE8:
            return 8;
    }
    throw std::runtime_error("unhandled index type (ngram)");
}

TokenValidator get_validator_for(IndexType type) {
    switch (type) {
        case IndexType::GRAM3:
            return [](uint32_t, uint8_t) { return true; };
        case IndexType::TEXT4:
            return
                [](uint32_t, uint8_t chr) { return get_b64_value(chr) >= 0; };
        case IndexType::HASH4:
            return [](uint32_t, uint8_t) { return true; };
        case IndexType::WIDE8:
            return [](uint32_t ndx, uint8_t chr) {
                if ((ndx % 2) == 0) {
                    return get_b64_value(chr) >= 0;
                }
                return chr == 0;
            };
    }
    throw std::runtime_error("unhandled index type (validator)");
}

TrigramGenerator get_generator_for(IndexType type) {
    switch (type) {
        case IndexType::GRAM3:
            return gen_trigrams;
        case IndexType::TEXT4:
            return gen_b64grams;
        case IndexType::HASH4:
            return gen_h4grams;
        case IndexType::WIDE8:
            return gen_wide_b64grams;
    }

    throw std::runtime_error("unhandled index type");
}

std::optional<TriGram> convert_gram(IndexType type, uint64_t source) {
    int size = get_ngram_size_for(type);
    std::vector<TriGram> result;
    std::vector<uint8_t> mem;
    mem.reserve(size);
    for (int i = 0; i < size; i++) {
        mem.push_back(((source) >> ((size - i - 1) * 8)) & 0xFF);
    }
    get_generator_for(type)(
        mem.data(), size, [&result](uint32_t gram) { result.push_back(gram); });
    if (result.empty()) {
        return std::nullopt;
    }
    return std::make_optional(result[0]);
}

std::optional<TriGram> convert_gram(IndexType type, int index,
                                    const QString &string) {
    int size = get_ngram_size_for(type);
    if (index + size > string.size()) {
        return std::nullopt;
    }
    uint64_t source = 0;
    for (int i = 0; i < size; i++) {
        if (!string[index + i].unique()) {
            return std::nullopt;
        }
        source = (source << 8) | string[index + i].possible_values()[0];
    }
    return convert_gram(type, source);
}

void gen_b64grams(const uint8_t *mem, uint64_t size,
                  const TrigramCallback &cb) {
    if (size < 4) {
        return;
    }

    uint32_t gram4 = 0;
    uint64_t good_run = 0;

    for (uint64_t offset = 0; offset < size; offset++) {
        int next = get_b64_value(mem[offset]);
        if (next < 0) {
            good_run = 0;
        } else {
            gram4 = ((gram4 << 6U) + next) & 0xFFFFFFU;
            good_run += 1;
        }
        if (good_run >= 4) {
            cb(gram4);
        }
    }
}

void gen_wide_b64grams(const uint8_t *mem, uint64_t size,
                       const TrigramCallback &cb) {
    if (size < 8) {
        return;
    }

    uint32_t gram4 = 0;
    uint64_t good_run = 0;

    for (uint64_t offset = 0; offset < size; offset++) {
        if (good_run % 2 == 1) {
            if (mem[offset] == 0) {
                good_run += 1;
            } else {
                good_run = 0;
            }

            if (good_run >= 8) {
                cb(gram4);
            }
        } else {
            int next = get_b64_value(mem[offset]);
            if (next < 0) {
                good_run = 0;
            } else {
                gram4 = ((gram4 << 6) + next) & 0xFFFFFFU;
                good_run += 1;
            }
        }
    }
}

void gen_trigrams(const uint8_t *mem, uint64_t size,
                  const TrigramCallback &cb) {
    if (size < 3) {
        return;
    }

    uint32_t gram3 = (mem[0] << 8U) | mem[1];

    for (uint64_t offset = 2; offset < size; offset++) {
        gram3 = ((gram3 & 0xFFFFU) << 8U) | mem[offset];
        cb(gram3);
    }
}

void gen_h4grams(const uint8_t *mem, uint64_t size, const TrigramCallback &cb) {
    if (size < 4) {
        return;
    }

    uint32_t gram4 = 0;

    for (uint64_t offset = 0; offset < size; offset++) {
        gram4 = ((gram4 & 0xFFFFFFU) << 8U) | mem[offset];

        if (offset >= 3) {
            cb(((gram4 >> 8U) & 0xFFFFFFU) ^ (gram4 & 0xFFFFFFU));
        }
    }
}

void RunWriter::write(FileId next) {
    assert(next > prev);
    int64_t diff = (next - prev) - 1;
    while (diff >= 0x80U) {
        out_bytes++;
        out->put(static_cast<uint8_t>(0x80U | (diff & 0x7FU)));
        diff >>= 7;
    }
    out_bytes++;
    out->put(static_cast<uint8_t>(diff));
    prev = next;
}

PosixRunWriter::~PosixRunWriter() {
    if (!buffer_.empty()) {
        // PosixRunWriter buffer not flushed, fatal error.
        spdlog::error("PosixRunWriter not flushed before destructing");
        std::terminate();
    }
}

// Arbitrary run buffer size for PosixRunWriter - 128kb.
const uint64_t RUN_BUFFER_SIZE = 128 * 1024 * 1024;

void PosixRunWriter::write(FileId next) {
    assert(next > prev_);
    int64_t diff = (next - prev_) - 1;
    while (diff >= 0x80U) {
        out_bytes_++;
        buffer_.push_back(static_cast<uint8_t>(0x80U | (diff & 0x7FU)));
        diff >>= 7;
    }
    out_bytes_++;
    buffer_.push_back(static_cast<uint8_t>(diff));
    prev_ = next;

    if (buffer_.size() > RUN_BUFFER_SIZE) {
        flush();
    }
}

FileId decompress_single(uint8_t **ptrptr) {
    uint32_t shift = 0;
    uint8_t *&ptr = *ptrptr;
    for (uint64_t acc = 0;; ptr++) {
        acc += (*ptr & 0x7FU) << shift;
        if ((*ptr & 0x80U) == 0) {
            ptr++;
            return acc;
        }
        shift += 7U;
    }
}

void PosixRunWriter::write_raw(FileId base, uint8_t *start,
                               const uint8_t *end) {
    // Special case: when the range is empty, do nothing.
    if (end == start) {
        return;
    }
    // Decompress the first element and write it.
    uint64_t next = decompress_single(&start);
    write(base + next);

    // Write all the other bytes unchanged, and compute the new last element.
    uint64_t acc = 0;
    uint32_t shift = 0;
    for (const uint8_t *ptr = start; ptr < end; ++ptr) {
        out_bytes_++;
        buffer_.push_back(*ptr);
        if (buffer_.size() > RUN_BUFFER_SIZE) {
            flush();
        }
        acc += (*ptr & 0x7FU) << shift;
        shift += 7U;
        if ((*ptr & 0x80U) == 0) {
            prev_ += acc + 1;
            acc = 0;
            shift = 0;
        }
    }
}

void PosixRunWriter::flush() {
    if (!buffer_.empty()) {
        if (::write(fd_, buffer_.data(), buffer_.size()) !=
            static_cast<int>(buffer_.size())) {
            throw std::runtime_error("Failed to flush PosixRunWriter");
        }
        buffer_.clear();
    }
}

uint64_t compress_run(const std::vector<FileId> &run, std::ostream &out) {
    RunWriter writer(&out);

    for (FileId next : run) {
        writer.write(next);
    }

    return writer.written_bytes();
}

std::vector<FileId> read_compressed_run(const uint8_t *start,
                                        const uint8_t *end) {
    std::vector<FileId> res;
    uint64_t acc = 0;
    uint32_t shift = 0;
    int64_t prev = -1;

    for (const uint8_t *ptr = start; ptr < end; ++ptr) {
        uint32_t next = *ptr;

        acc += (next & 0x7FU) << shift;
        shift += 7U;
        if ((next & 0x80U) == 0) {
            prev += acc + 1;
            res.push_back(prev);
            acc = 0;
            shift = 0;
        }
    }

    return res;
}

void store_dataset(const fs::path &db_base, const std::string &fname,
                   const std::set<std::string> &index_names,
                   const std::string &fname_list,
                   std::optional<std::string_view> fname_cache,
                   const std::set<std::string> &taints) {
    json dataset;
    json j_indices(index_names);

    dataset["indices"] = j_indices;
    dataset["files"] = fname_list;
    dataset["taints"] = taints;

    if (fname_cache) {
        dataset["filename_cache"] = std::string(*fname_cache);
    }

    std::ofstream o;
    o.exceptions(std::ofstream::badbit);
    o.open(db_base / fname, std::ofstream::binary);

    o << std::setw(4) << dataset << std::endl;
    o.flush();
}

std::string bin_str_to_hex(const std::string &str) {
    std::ostringstream ret;

    for (uint8_t i : str) {
        uint32_t c = static_cast<uint32_t>(i);
        ret << std::hex << std::setfill('0') << std::setw(2) << std::uppercase
            << c;
    }
    return ret.str();
}
