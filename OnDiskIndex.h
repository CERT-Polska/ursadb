#pragma once

#include <vector>
#include <string>
#include <fstream>
#include "Core.h"
#include "MemMap.h"

struct IndexMergeHelper;

class OnDiskIndex {
    const uint32_t *run_offsets;
    MemMap disk_map;
    IndexType ntype;

    const uint8_t *data() const { return disk_map.data(); }
    static constexpr uint32_t VERSION = 5;

public:
    explicit OnDiskIndex(const std::string &fname);

    IndexType index_type() const { return ntype; }
    std::vector<FileId> query_primitive(TriGram trigram) const;
    static void on_disk_merge(std::string fname, IndexType merge_type, const std::vector<IndexMergeHelper> &indexes);
};

struct IndexMergeHelper {
    const OnDiskIndex *index;
    uint32_t file_count;

    IndexMergeHelper(const OnDiskIndex *index, uint32_t file_count)
        :index(index), file_count(file_count) {}
};