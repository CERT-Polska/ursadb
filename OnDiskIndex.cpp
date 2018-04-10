#include <iostream>
#include "OnDiskIndex.h"


OnDiskIndex::OnDiskIndex(const std::string &fname) : run_offsets(NUM_TRIGRAMS) {
    raw_data = std::ifstream(fname, std::ifstream::binary | std::ifstream::ate);
    long fsize = raw_data.tellg();
    raw_data.seekg(0, std::ifstream::beg);

    uint32_t magic;
    uint32_t version;
    uint32_t reserved;

    raw_data.read((char*)&magic, 4);

    if (magic != DB_MAGIC) {
        throw std::runtime_error("invalid magic, not a catdata");
    }

    raw_data.read((char*)&version, 4);

    if (version != 5) {
        throw std::runtime_error("unsupported version");
    }

    raw_data.read((char*)&ntype, 4);

    if (ntype != GRAM3) {
        throw std::runtime_error("invalid index type");
    }

    raw_data.read((char*)&reserved, 4);

    raw_data.seekg(fsize - NUM_TRIGRAMS*4, std::ifstream::beg);
    raw_data.read((char*)&run_offsets[0], NUM_TRIGRAMS*4);
}


std::vector<FileId> OnDiskIndex::read_compressed_run(std::ifstream &runs, size_t len) {
    std::vector<FileId> res;
    uint32_t acc = 0;
    uint32_t shift = 0;
    uint32_t base = 0;

    for (int i = 0; i < len; i++) {
        char next_raw;
        runs.read(&next_raw, 1);
        uint32_t next = next_raw;

        acc += (next & 0x7FU) << shift;
        shift += 7U;
        if ((next & 0x80U) == 0) {
            base += acc;
            res.push_back(base - 1U);
            acc = 0;
            shift = 0;
        }
    }

    return res;
}


std::vector<FileId> OnDiskIndex::query_primitive(const TriGram &trigram) {
    uint32_t ptr = run_offsets[trigram];
    uint32_t next_ptr = run_offsets[trigram+1];
    // TODO(_): check for overflow
    // Note: it's also possible to increase run_offsets size by 1.

    raw_data.seekg(ptr);
    return read_compressed_run(raw_data, next_ptr - ptr);
}
