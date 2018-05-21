#pragma once

#include <string>

#include "OnDiskDataset.h"

enum class MergeStrategy {
    // try to optimize (useful for index)
    Smart = 1,
    // preserve ordering of FileIds (useful for reindex)
    InOrder = 2
};

class Indexer {
    MergeStrategy strategy;
    const DatabaseSnapshot *snap;
    std::vector<IndexType> types;
    DatasetBuilder builder;
    std::vector<std::unique_ptr<OnDiskDataset>> created_datasets;

    std::vector<const OnDiskDataset *> created_dataset_ptrs();
    std::vector<const OnDiskDataset *> get_merge_candidates();
    void make_spill();
    void register_dataset(const std::string &dataset_name);
    void remove_dataset(const OnDiskDataset *dataset_ptr);

public:
    Indexer(MergeStrategy strategy, const DatabaseSnapshot *snap, const std::vector<IndexType> &types);
    void index(const std::string &target);
    OnDiskDataset *force_compact();
    std::vector<const OnDiskDataset *> finalize();
};
