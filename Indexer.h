#pragma once

#include <string>

#include "OnDiskDataset.h"

enum MergeStrategy {
    // try to optimize (useful for index)
    Smart = 1,
    // preserve ordering of FileIds (useful for reindex)
    InOrder = 2
};

class Indexer {
    MergeStrategy strategy;
    const DatabaseSnapshot *snap;
    Task *task;
    DatasetBuilder builder;
    std::map<std::string, std::unique_ptr<OnDiskDataset>> created_datasets;
    std::vector<IndexType> types;

    std::vector<const OnDiskDataset *> created_dataset_ptrs();
    std::vector<OnDiskDataset *> created_nonconst_dataset_ptrs();
    std::vector<OnDiskDataset *> get_merge_candidates();
    void make_spill();
    void register_dataset(const std::string &dataset_name);
    void remove_datasets(const std::vector<OnDiskDataset *> &datasets);

public:
    Indexer(MergeStrategy strategy, const DatabaseSnapshot *snap, Task *task, const std::vector<IndexType> &types);
    void index(const std::string &target);
    OnDiskDataset *force_compact();
    std::vector<const OnDiskDataset *> finalize();
};
