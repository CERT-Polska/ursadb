#include "Indexer.h"

Indexer::Indexer(MergeStrategy strategy, const DatabaseSnapshot *snap, Task *task, const std::vector<IndexType> &types)
        : strategy(strategy), snap(snap), task(task), types(types), builder(types) {

}

void Indexer::index(const std::string &target) {
    try {
        builder.index(target);
    } catch (empty_file_error &e) {
        std::cout << "empty file, skip" << std::endl;
    }

    if (builder.must_spill()) {
        make_spill();
    }
}

std::vector<const OnDiskDataset *> Indexer::created_dataset_ptrs() {
    std::vector<const OnDiskDataset *> ptrs;

    for (auto &p : created_datasets) {
        ptrs.push_back(p.second.get());
    }

    return ptrs;
}

std::vector<OnDiskDataset *> Indexer::created_nonconst_dataset_ptrs() {
    // TODO force order when strategy is InOrder

    std::vector<OnDiskDataset *> ptrs;

    for (auto &p : created_datasets) {
        ptrs.push_back(p.second.get());
    }

    return ptrs;
}

void Indexer::register_dataset(const std::string &dataset_name) {
    created_datasets.emplace(dataset_name, std::make_unique<OnDiskDataset>(snap->db_base, dataset_name));
}

std::vector<OnDiskDataset *> Indexer::get_merge_candidates() {
    if (strategy == MergeStrategy::Smart) {
        return OnDiskDataset::get_compact_candidates(created_nonconst_dataset_ptrs());
    } else if (strategy == MergeStrategy::InOrder) {
        // TODO could be improved
        if (created_datasets.size() >= 2) {
            return created_nonconst_dataset_ptrs();
        } else {
            return {};
        }
    } else {
        throw std::runtime_error("unhandled merge strategy");
    }
}

void Indexer::make_spill() {
    std::cout << "new dataset" << std::endl;
    auto dataset_name = snap->allocate_name();
    builder.save(snap->db_base, dataset_name);
    register_dataset(dataset_name);
    bool stop = false;

    while (!stop) {
        std::vector<OnDiskDataset *> candidates = get_merge_candidates();

        if (!candidates.empty()) {
            std::cout << "merge stuff" << std::endl;
            std::string merged_name = snap->allocate_name();
            OnDiskDataset::merge(snap->db_base, merged_name, {candidates.begin(), candidates.end()}, task);
            remove_datasets({candidates.begin(), candidates.end()});
            register_dataset(merged_name);
        } else {
            std::cout << "not going to merge" << std::endl;
            stop = true;
        }
    }

    builder = DatasetBuilder(types);
}

void Indexer::remove_datasets(const std::vector<OnDiskDataset *> &candidates) {
    for (auto *ds : candidates) {
        ds->drop();
        created_datasets.erase(ds->get_name());
    }
}

OnDiskDataset *Indexer::force_compact() {
    if (!builder.empty()) {
        make_spill();
    }

    if (created_datasets.empty()) {
        throw std::runtime_error("forced to compact but no single file was indexed");
    }

    if (created_datasets.size() > 1) {
        std::vector<OnDiskDataset *> candidates = created_nonconst_dataset_ptrs();
        std::string merged_name = snap->allocate_name();
        OnDiskDataset::merge(snap->db_base, merged_name, {candidates.begin(), candidates.end()}, task);
        remove_datasets({candidates.begin(), candidates.end()});
        register_dataset(merged_name);
    }

    return (*created_datasets.begin()).second.get();
}

std::vector<const OnDiskDataset *> Indexer::finalize() {
    if (!builder.empty()) {
        make_spill();
    }

    return created_dataset_ptrs();
}
