#pragma once

#include <vector>
#include <string>
#include <fstream>
#include "Core.h"
#include "MemMap.h"

enum IndexType {
    GRAM3 = 1
};

class OnDiskIndex {
    uint32_t *run_offsets;
    MemMap disk_map;
    IndexType ntype;

    std::vector<FileId> read_compressed_run(const uint8_t *start, const uint8_t *end) const;

    static constexpr uint32_t VERSION = 5;

public:
    explicit OnDiskIndex(const std::string &fname);

    std::vector<FileId> query_primitive(TriGram trigram) const;
};
