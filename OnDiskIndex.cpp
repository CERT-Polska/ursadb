#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include "OnDiskIndex.h"


OnDiskIndex::OnDiskIndex(const std::string &fname) : disk_map(fname) {
    uint32_t magic = *(uint32_t *) &disk_map[0];
    uint32_t version = *(uint32_t *) &disk_map[4];
    ntype = static_cast<IndexType>(*(uint32_t *) &disk_map[8]);
    uint32_t reserved = *(uint32_t *) &disk_map[12];

    if (magic != DB_MAGIC) {
        throw std::runtime_error("invalid magic, not a catdata");
    }

    if (version != OnDiskIndex::VERSION) {
        throw std::runtime_error("unsupported version");
    }

    if (ntype != GRAM3) {
        throw std::runtime_error("invalid index type");
    }

    run_offsets = (uint32_t*) &disk_map[disk_map.size() - (NUM_TRIGRAMS + 1) * 4];
}


std::vector<FileId> OnDiskIndex::read_compressed_run(const uint8_t *start, const uint8_t *end) const {
    std::vector<FileId> res;
    uint32_t acc = 0;
    uint32_t shift = 0;
    uint32_t base = 0;

    for (const uint8_t *ptr = start; ptr < end; ++ptr) {
        uint32_t next = *ptr;

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


std::vector<FileId> OnDiskIndex::query_primitive(TriGram trigram) const {
    uint32_t ptr = run_offsets[trigram];
    uint32_t next_ptr = run_offsets[trigram + 1];

    if (ptr == next_ptr) {
        std::cout << "empty index for " << trigram << std::endl;
    } else {
        std::cout << "for " << trigram << ": " << (next_ptr - ptr) << std::endl;
    }

    std::vector<FileId> out = read_compressed_run(&disk_map[ptr], &disk_map[next_ptr]);
    std::cout << "returning " << out.size() << " elems" << std::endl;
    return out;
}
