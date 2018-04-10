#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "Core.h"

enum IndexType {
    GRAM3 = 1
};

class OnDiskIndex {
    std::vector<uint32_t> run_offsets;
    uint8_t *mmap_ptr;
    IndexType ntype;

    std::vector<FileId> read_compressed_run(uint8_t *start, uint8_t *end);

public:
    OnDiskIndex(const std::string &fname);
    std::vector<FileId> query_primitive(const TriGram &trigram);
};
