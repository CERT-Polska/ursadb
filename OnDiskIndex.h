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
    const uint32_t *run_offsets;
    MemMap disk_map;
    IndexType ntype;

    static constexpr uint32_t VERSION = 5;

public:
    explicit OnDiskIndex(const std::string &fname);

    std::vector<FileId> query_primitive(TriGram trigram) const;
};
