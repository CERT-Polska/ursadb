#pragma once

#include "Core.h"
#include "MemMap.h"
#include "Query.h"
#include <fstream>
#include <string>
#include <vector>

struct IndexMergeHelper;

class OnDiskIndex {
    const uint32_t *run_offsets;
    MemMap disk_map;
    IndexType ntype;

    const uint8_t *data() const { return disk_map.data(); }
    static constexpr uint32_t VERSION = 5;
    std::vector<FileId> query_primitive(TriGram trigram) const;

  public:
    explicit OnDiskIndex(const std::string &fname);

    const std::string &get_fname() const { return disk_map.name(); }
    IndexType index_type() const { return ntype; }
    QueryResult query_str(const std::string &str) const;
    static void on_disk_merge(std::string fname, IndexType merge_type,
                              const std::vector<IndexMergeHelper> &indexes);
};

struct IndexMergeHelper {
    const OnDiskIndex *index;
    uint32_t file_count;

    IndexMergeHelper(const OnDiskIndex *index, uint32_t file_count)
        : index(index), file_count(file_count) {}
};