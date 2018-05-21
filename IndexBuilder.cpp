#include "IndexBuilder.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <cassert>

#include "Utils.h"

constexpr int max_files = 8*8;
constexpr int file_run_size = max_files / 8;


IndexBuilder::IndexBuilder(IndexType ntype)
    : raw_data(file_run_size * NUM_TRIGRAMS), ntype(ntype) {}

void IndexBuilder::add_trigram(FileId fid, TriGram val) {
    unsigned int offset = fid / 8;
    unsigned int shift = fid % 8;
    raw_data[val * file_run_size + offset] |= (1U << shift);
}

std::vector<FileId> IndexBuilder::get_run(TriGram val) const {
    unsigned int run_start = file_run_size * val;
    std::vector<FileId> result;
    for (int offset = 0; offset < file_run_size; offset++) {
        for (int shift = 0; shift < 8; shift++) {
            if (raw_data[run_start + offset] & (1 << shift)) {
                result.push_back(offset * 8 + shift);
            }
        }
    }
    return result;
}

void IndexBuilder::save(const std::string &fname) const {
    std::ofstream out(fname, std::ofstream::binary | std::ofstream::out);

    uint32_t magic = DB_MAGIC;
    uint32_t version = 6;
    uint32_t ndx_type = static_cast<uint32_t>(ntype);
    uint32_t reserved = 0;

    out.write((char *)&magic, 4);
    out.write((char *)&version, 4);
    out.write((char *)&ndx_type, 4);
    out.write((char *)&reserved, 4);

    auto offset = (uint64_t)out.tellp();
    std::vector<uint64_t> offsets(NUM_TRIGRAMS + 1);

    for (TriGram i = 0; i < NUM_TRIGRAMS; i++) {
        offsets[i] = offset;
        offset += compress_run(get_run(i), out);
    }

    offsets[NUM_TRIGRAMS] = offset;

    out.write((char *)offsets.data(), (NUM_TRIGRAMS + 1) * sizeof(uint64_t));
}

void IndexBuilder::add_file(FileId fid, const uint8_t *data, size_t size) {
    if (fid >= max_files) {
        // IndexBuilder's bitmap can't hold more than max_files files
        throw std::out_of_range("fid");
    }

    TrigramGenerator generator = get_generator_for(ntype);
    generator(data, size, [&](TriGram val) { add_trigram(fid, val); });
}

bool IndexBuilder::must_spill(int file_count) const {
    return file_count >= max_files;
}