#pragma once

#include <string>

#include "OnDiskDataset.h"

class Indexer {
    const DatabaseSnapshot *snap;
    std::vector<IndexType> types;
    DatasetBuilder flat_builder;
    DatasetBuilder bitmap_builder;
    std::vector<std::unique_ptr<OnDiskDataset>> created_datasets;

    std::vector<const OnDiskDataset *> created_dataset_ptrs();
    void make_spill(DatasetBuilder &builder);
    void register_dataset(const std::string &dataset_name);
    void remove_dataset(const OnDiskDataset *dataset_ptr);

   public:
    Indexer(const DatabaseSnapshot *snap, const std::vector<IndexType> &types);
    void index(const std::string &target);
    OnDiskDataset *force_compact();
    std::vector<const OnDiskDataset *> finalize();
};
