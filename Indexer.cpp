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
        ptrs.push_back(p.get());
    }

    return ptrs;
}

void Indexer::register_dataset(const std::string &dataset_name) {
    created_datasets.push_back(std::make_unique<OnDiskDataset>(snap->db_base, dataset_name));
}

void Indexer::remove_dataset(const OnDiskDataset *dataset_ptr) {
    for (auto it = created_datasets.begin(); it != created_datasets.end(); ) {
        if ((*it).get() == dataset_ptr) {
            (*it)->drop();
            it = created_datasets.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<const OnDiskDataset *> Indexer::get_merge_candidates() {
    if (strategy == MergeStrategy::Smart) {
        return OnDiskDataset::get_compact_candidates(created_dataset_ptrs());
    } else if (strategy == MergeStrategy::InOrder) {
        // TODO could be improved
        if (created_datasets.size() >= 2) {
            return created_dataset_ptrs();
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
        std::vector<const OnDiskDataset *> candidates = get_merge_candidates();

        if (!candidates.empty()) {
            std::cout << "merge stuff" << std::endl;
            std::string merged_name = snap->allocate_name();
            OnDiskDataset::merge(snap->db_base, merged_name, candidates, task);
            for (const auto *ds : candidates) {
                remove_dataset(ds);
            }
            register_dataset(merged_name);
        } else {
            std::cout << "not going to merge" << std::endl;
            stop = true;
        }
    }

    builder = DatasetBuilder(types);
}

OnDiskDataset *Indexer::force_compact() {
    if (!builder.empty()) {
        make_spill();
    }

    if (created_datasets.empty()) {
        throw std::runtime_error("forced to compact but no single file was indexed");
    }

    if (created_datasets.size() > 1) {
        std::vector<const OnDiskDataset *> candidates = created_dataset_ptrs();
        std::string merged_name = snap->allocate_name();
        OnDiskDataset::merge(snap->db_base, merged_name, {candidates.begin(), candidates.end()}, task);
        for (const auto *ds : candidates) {
            remove_dataset(ds);
        }
        register_dataset(merged_name);
    }

    return (*created_datasets.begin()).get();
}

std::vector<const OnDiskDataset *> Indexer::finalize() {
    if (!builder.empty()) {
        make_spill();
    }

    return created_dataset_ptrs();
}
