#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include "OnDiskIndex.h"


OnDiskIndex::OnDiskIndex(const std::string &fname) : run_offsets(NUM_TRIGRAMS) {
    int fd = open(fname.c_str(), O_RDONLY, (mode_t) 0600);
    auto fsize = static_cast<size_t>(lseek(fd, 0, SEEK_END));
    mmap_ptr = (uint8_t *) mmap(nullptr, fsize, PROT_READ, MAP_SHARED, fd, 0);

    uint32_t magic = *(uint32_t *) mmap_ptr;
    uint32_t version = *(uint32_t *) (mmap_ptr + 4);
    ntype = static_cast<IndexType>(*(uint32_t *) (mmap_ptr + 8));
    uint32_t reserved = *(uint32_t *) (mmap_ptr + 12);

    if (magic != DB_MAGIC) {
        throw std::runtime_error("invalid magic, not a catdata");
    }

    if (version != 5) {
        throw std::runtime_error("unsupported version");
    }

    if (ntype != GRAM3) {
        throw std::runtime_error("invalid index type");
    }

    memcpy(&run_offsets[0], &mmap_ptr[fsize - NUM_TRIGRAMS * 4], NUM_TRIGRAMS * 4);
}


std::vector<FileId> OnDiskIndex::read_compressed_run(uint8_t *start, uint8_t *end) {
    std::vector<FileId> res;
    uint32_t acc = 0;
    uint32_t shift = 0;
    uint32_t base = 0;

    for (uint8_t *ptr = start; ptr < end; ++ptr) {
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


std::vector<FileId> OnDiskIndex::query_primitive(const TriGram &trigram) {
    uint32_t ptr = run_offsets[trigram];
    uint32_t next_ptr = run_offsets[trigram + 1];
    // TODO(_): check for overflow
    // Note: it's also possible to increase run_offsets size by 1.

    return read_compressed_run(&mmap_ptr[ptr], &mmap_ptr[next_ptr]);
}
