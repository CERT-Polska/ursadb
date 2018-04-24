#include <fstream>
#include <set>

#include "MemMap.h"
#include "Utils.h"
#include "lib/Json.h"

using json = nlohmann::json;

TrigramGenerator get_generator_for(IndexType type) {
    switch (type) {
        case IndexType::GRAM3:
            return get_trigrams;
        case IndexType::TEXT4:
            return get_b64grams;
        case IndexType::HASH4:
            return get_h4grams;
    }
}

std::vector<TriGram> get_b64grams(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    if (size < 4) {
        return out;
    }

    uint32_t gram4 = 0;
    int good_run = 0;

    for (int offset = 0; offset < size; offset++) {
        int next = get_b64_value(mem[offset]);
        if (next < 0) {
            good_run = 0;
        } else {
            gram4 = ((gram4 << 6) + next) & 0xFFFFFF;
            good_run += 1;
        }
        if (good_run >= 4) {
            out.push_back(gram4);
        }
    }

    return out;
}

std::vector<TriGram> get_trigrams(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    if (size < 3) {
        return out;
    }

    uint32_t gram3 = (mem[0] << 8U) | mem[1];

    for (int offset = 2; offset < size; offset++) {
        gram3 = ((gram3 & 0xFFFFU) << 8U) | mem[offset];
        out.push_back(gram3);
    }

    return out;
}

std::vector<TriGram> get_h4grams(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    if (size < 4) {
        return out;
    }

    uint32_t gram4 = 0;

    for (int offset = 0; offset < size; offset++) {
        gram4 = ((gram4 & 0xFFFFFFU) << 8U) | mem[offset];

        if (offset >= 3) {
            out.push_back(((gram4 >> 8U) & 0xFFFFFFU) ^ (gram4 & 0xFFFFFFU));
        }
    }

    return out;
}

void compress_run(const std::vector<FileId> &run, std::ostream &out) {
    int64_t prev = -1;

    for (FileId next : run) {
        int64_t diff = (next - prev) - 1;
        while (diff >= 0x80U) {
            out.put((uint8_t)(0x80U | (diff & 0x7FU)));
            diff >>= 7;
        }
        out.put((uint8_t)diff);
        prev = next;
    }
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
    }
}

void store_dataset(const std::string &fname, std::set<std::string> index_names, std::vector<std::string> &fids) {
    std::string fname_list = "files." + fname;
    std::ofstream of(fname_list, std::ofstream::out);
    for (auto &fn : fids) {
        of << fn << std::endl;
    }
    of.close();

    json dataset;
    json j_indices(index_names);

    dataset["indices"] = j_indices;
    dataset["filename_list"] = fname_list;

    std::ofstream o(fname, std::ofstream::out);
    o << std::setw(4) << dataset << std::endl;
    o.close();
}
