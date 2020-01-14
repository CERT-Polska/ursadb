#include "Utils.h"

#include <random>
#include <fstream>
#include <set>

#include "Json.h"

std::string random_hex_string(unsigned long length) {
    constexpr static char charset[] = "0123456789abcdef";
    thread_local static std::random_device rd;
    thread_local static std::seed_seq seed{rd(), rd(), rd(), rd()}; // A bit better than pathetic default
    thread_local static std::mt19937_64 random(seed);
    thread_local static std::uniform_int_distribution<int> pick(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);

    for (unsigned long i = 0; i < length; i++) {
        result += charset[pick(random)];
    }

    return result;
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

void gen_b64grams(const uint8_t *mem, size_t size, TrigramCallback cb) {
    if (size < 4) {
        return;
    }

    uint32_t gram4 = 0;
    int good_run = 0;

    for (size_t offset = 0; offset < size; offset++) {
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

void gen_wide_b64grams(const uint8_t *mem, size_t size, TrigramCallback cb) {
    if (size < 8) {
        return;
    }

    uint32_t gram4 = 0;
    int good_run = 0;

    for (unsigned int offset = 0; offset < size; offset++) {
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

void gen_trigrams(const uint8_t *mem, size_t size, TrigramCallback cb) {
    if (size < 3) {
        return;
    }

    uint32_t gram3 = (mem[0] << 8U) | mem[1];

    for (unsigned int offset = 2; offset < size; offset++) {
        gram3 = ((gram3 & 0xFFFFU) << 8U) | mem[offset];
        cb(gram3);
    }
}

void gen_h4grams(const uint8_t *mem, size_t size, TrigramCallback cb) {
    if (size < 4) {
        return;
    }

    uint32_t gram4 = 0;

    for (unsigned int offset = 0; offset < size; offset++) {
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
        out->put((uint8_t)(0x80U | (diff & 0x7FU)));
        diff >>= 7;
    }
    out_bytes++;
    out->put((uint8_t)diff);
    prev = next;
}

uint64_t compress_run(const std::vector<FileId> &run, std::ostream &out) {
    RunWriter writer(&out);

    for (FileId next : run) {
        writer.write(next);
    }

    return writer.written_bytes();
}

std::vector<FileId> read_compressed_run(const uint8_t *start, const uint8_t *end) {
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

std::string get_index_type_name(IndexType type) {
    switch (type) {
        case IndexType::GRAM3:
            return "gram3";
        case IndexType::TEXT4:
            return "text4";
        case IndexType::HASH4:
            return "hash4";
        case IndexType::WIDE8:
            return "wide8";
    }

    throw std::runtime_error("unhandled index type");
}

std::optional<IndexType> index_type_from_string(const std::string &type) {
    if (type == "gram3") {
        return IndexType::GRAM3;
    } else if (type == "text4") {
        return IndexType::TEXT4;
    } else if (type == "hash4") {
        return IndexType::HASH4;
    } else if (type == "wide8") {
        return IndexType::WIDE8;
    } else {
        return std::nullopt;
    }
}

void store_dataset(
        const fs::path &db_base,
        const std::string &fname,
        const std::set<std::string> &index_names,
        const std::string &fname_list) {
    json dataset;
    json j_indices(index_names);

    dataset["indices"] = j_indices;
    dataset["files"] = fname_list;

    std::ofstream o;
    o.exceptions(std::ofstream::badbit);
    o.open(db_base / fname, std::ofstream::binary);

    o << std::setw(4) << dataset << std::endl;
    o.flush();
}

std::string bin_str_to_hex(const std::string& str) {
    std::ostringstream ret;

    unsigned int c;
    for (std::string::size_type i = 0; i < str.length(); ++i)
    {
        c = (unsigned int)(unsigned char)str[i];
        ret << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << c;
    }
    return ret.str();
}
