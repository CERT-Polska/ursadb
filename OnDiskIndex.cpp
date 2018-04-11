#include "OnDiskIndex.h"

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include "Utils.h"

OnDiskIndex::OnDiskIndex(const std::string &fname) : disk_map(fname) {
    const uint8_t *data = disk_map.data();
    uint32_t magic = *(uint32_t *) &data[0];
    uint32_t version = *(uint32_t *) &data[4];
    ntype = static_cast<IndexType>(*(uint32_t *) &data[8]);
    uint32_t reserved = *(uint32_t *) &data[12];

    if (magic != DB_MAGIC) {
        throw std::runtime_error("invalid magic, not a catdata");
    }

    if (version != OnDiskIndex::VERSION) {
        throw std::runtime_error("unsupported version");
    }

    if (ntype != GRAM3) {
        throw std::runtime_error("invalid index type");
    }

    run_offsets = (uint32_t*) &data[disk_map.size() - (NUM_TRIGRAMS + 1) * 4];
}

std::vector<FileId> OnDiskIndex::query_primitive(TriGram trigram) const {
    uint32_t ptr = run_offsets[trigram];
    uint32_t next_ptr = run_offsets[trigram + 1];

    if (ptr == next_ptr) {
        std::cout << "empty index for " << trigram << std::endl;
    } else {
        std::cout << "for " << trigram << ": " << (next_ptr - ptr) << std::endl;
    }

    const uint8_t *data = disk_map.data();
    std::vector<FileId> out = read_compressed_run(&data[ptr], &data[next_ptr]);
    std::cout << "returning " << out.size() << " elems" << std::endl;
    return out;
}
