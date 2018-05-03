#include "Utils.h"

#include <fstream>
#include <set>

#include "MemMap.h"
#include "lib/Json.h"

using json = nlohmann::json;
namespace fs = std::experimental::filesystem;

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
}

void gen_b64grams(const uint8_t *mem, size_t size, TrigramCallback cb) {
    if (size < 4) {
        return;
    }

    uint32_t gram4 = 0;
    int good_run = 0;

    for (int offset = 0; offset < size; offset++) {
        int next = get_b64_value(mem[offset]);
        if (next < 0) {
            good_run = 0;
        } else {
            gram4 = ((gram4 << 6U) + next) & 0xFFFFFF;
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

    for (int offset = 0; offset < size; offset++) {
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
                gram4 = ((gram4 << 6) + next) & 0xFFFFFF;
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

    for (int offset = 2; offset < size; offset++) {
        gram3 = ((gram3 & 0xFFFFU) << 8U) | mem[offset];
        cb(gram3);
    }
}

void gen_h4grams(const uint8_t *mem, size_t size, TrigramCallback cb) {
    if (size < 4) {
        return;
    }

    uint32_t gram4 = 0;

    for (int offset = 0; offset < size; offset++) {
        gram4 = ((gram4 & 0xFFFFFFU) << 8U) | mem[offset];

        if (offset >= 3) {
            cb(((gram4 >> 8U) & 0xFFFFFFU) ^ (gram4 & 0xFFFFFFU));
        }
    }
}

uint64_t compress_run(const std::vector<FileId> &run, std::ostream &out) {
    // be careful there, std::vector<FileId> must contain sorted and unique values
    uint64_t out_bytes = 0;
    int64_t prev = -1;

    for (FileId next : run) {
        int64_t diff = (next - prev) - 1;
        while (diff >= 0x80U) {
            out_bytes++;
            out.put((uint8_t)(0x80U | (diff & 0x7FU)));
            diff >>= 7;
        }
        out_bytes++;
        out.put((uint8_t)diff);
        prev = next;
    }

    return out_bytes;
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
        const fs::path &db_base, const std::string &fname, const std::set<std::string> &index_names,
        const std::vector<std::string> &fids) {
    std::string fname_list = "files." + fname;
    std::ofstream of(db_base / fname_list, std::ofstream::out | std::ofstream::binary);
    for (auto &fn : fids) {
        of << fn << "\n";
    }

    json dataset;
    json j_indices(index_names);

    dataset["indices"] = j_indices;
    dataset["files"] = fname_list;

    std::ofstream o(db_base / fname, std::ofstream::out | std::ofstream::binary);
    o << std::setw(4) << dataset << std::endl;
}