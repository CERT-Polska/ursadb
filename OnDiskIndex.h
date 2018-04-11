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

    const uint8_t *data() const { return disk_map.data(); }
    static constexpr uint32_t VERSION = 5;

public:
    explicit OnDiskIndex(const std::string &fname);

    std::vector<FileId> query_primitive(TriGram trigram) const;
    static void on_disk_merge(std::string fname, IndexType merge_type, std::vector<OnDiskIndex> indexes);
};
