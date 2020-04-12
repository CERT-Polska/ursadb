#include "Indexer.h"

#include "Core.h"
#include "MemMap.h"
#include "spdlog/spdlog.h"

Indexer::Indexer(const DatabaseSnapshot *snap,
                 const std::vector<IndexType> &types)
    : snap(snap),
      types(types),
      flat_builder(BuilderType::FLAT, types),
      bitmap_builder(BuilderType::BITMAP, types) {}

void Indexer::index(const std::string &target) {
    DatasetBuilder *builder = &flat_builder;

    try {
        uint64_t file_size = fs::file_size(target);

        if (file_size > 1024 * 1024 * 20) {
            builder = &bitmap_builder;
        }

        if (!builder->can_still_add(file_size)) {
            make_spill(*builder);
        }

        builder->index(target);
    } catch (empty_file_error &e) {
        spdlog::debug("Empty file (skip): {}", target);
    } catch (file_open_error &e) {
        spdlog::warn("Failed to open {} reason: {}", target, e.what());
    } catch (invalid_filename_error &e) {
        spdlog::warn("Illegal file name (skip): {}", target);
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
    created_datasets.emplace_back(
        std::make_unique<OnDiskDataset>(snap->db_base, dataset_name));
}

void Indexer::remove_dataset(const OnDiskDataset *dataset_ptr) {
    for (auto it = created_datasets.begin(); it != created_datasets.end();) {
        if ((*it).get() == dataset_ptr) {
            (*it)->drop();
            it = created_datasets.erase(it);
        } else {
            ++it;
        }
    }
}

void Indexer::make_spill(DatasetBuilder &builder) {
    auto dataset_name = snap->allocate_name().get_filename();
    spdlog::debug("New dataset: {}", dataset_name);
    builder.save(snap->db_base, dataset_name);
    register_dataset(dataset_name);
    bool stop = false;

    while (!stop) {
        std::vector<const OnDiskDataset *> candidates =
            OnDiskDataset::get_compact_candidates(created_dataset_ptrs());

        if (candidates.size() >= INDEXER_COMPACT_THRESHOLD) {
            spdlog::debug("Merging datasets");
            std::string merged_name = snap->allocate_name().get_filename();
            OnDiskDataset::merge(snap->db_base, merged_name, candidates,
                                 nullptr);
            for (const auto *ds : candidates) {
                remove_dataset(ds);
            }
            register_dataset(merged_name);
        } else {
            spdlog::debug("{} datasets, not merging", candidates.size());
            stop = true;
        }
    }

    builder.clear();
}

OnDiskDataset *Indexer::force_compact() {
    if (!flat_builder.empty()) {
        make_spill(flat_builder);
    }

    if (!bitmap_builder.empty()) {
        make_spill(bitmap_builder);
    }

    if (created_datasets.empty()) {
        throw std::runtime_error(
            "forced to compact but no single file was indexed");
    }

    if (created_datasets.size() > 1) {
        std::vector<const OnDiskDataset *> candidates = created_dataset_ptrs();
        std::string merged_name = snap->allocate_name().get_filename();
        OnDiskDataset::merge(snap->db_base, merged_name, candidates, nullptr);
        for (const auto *ds : candidates) {
            remove_dataset(ds);
        }
        register_dataset(merged_name);
    }

    return (*created_datasets.begin()).get();
}

std::vector<const OnDiskDataset *> Indexer::finalize() {
    if (!flat_builder.empty()) {
        make_spill(flat_builder);
    }

    if (!bitmap_builder.empty()) {
        make_spill(bitmap_builder);
    }

    return created_dataset_ptrs();
}
