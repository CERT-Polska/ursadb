#include "DatabaseSnapshot.h"

#include <fstream>

#include "Database.h"
#include "DatasetBuilder.h"
#include "ExclusiveFile.h"
#include "Json.h"
#include "Indexer.h"

DatabaseSnapshot::DatabaseSnapshot(
        fs::path db_name, fs::path db_base, std::vector<const OnDiskDataset *> datasets,
        const std::map<uint64_t, std::unique_ptr<Task>> &tasks, size_t max_memory_size)
    : db_name(db_name), db_base(db_base), datasets(datasets), tasks(),
      max_memory_size(max_memory_size) {
    for (const auto &entry : tasks) {
        this->tasks.emplace(entry.first, *entry.second.get());
    }
}

void DatabaseSnapshot::build_target_list(
        const std::string &filepath, const std::set<std::string> &existing_files,
        std::vector<std::string> &targets) const {
    fs::recursive_directory_iterator end;

    if (fs::is_regular_file(filepath)) {
        fs::path absfn = fs::absolute(filepath);

        if (existing_files.count(absfn) == 0) {
            targets.push_back(absfn);
        }
    } else {
        for (fs::recursive_directory_iterator dir(filepath); dir != end; ++dir) {
            if (fs::is_regular_file(dir->path())) {
                fs::path absfn = fs::absolute(dir->path());
                if (existing_files.count(absfn) == 0) {
                    targets.push_back(absfn);
                }
            }
        }
    }
}

void DatabaseSnapshot::index_path(
        Task *task, const std::vector<IndexType> &types,
        const std::vector<std::string> &filepaths) const {
    std::set<std::string> existing_files;

    for (const auto &ds : datasets) {
        for (const auto &fname : ds->indexed_files()) {
            existing_files.insert(fname);
        }
    }

    std::vector<std::string> targets;

    for (const auto &filepath : filepaths) {
        build_target_list(filepath, existing_files, targets);
    }

    auto last = std::unique(targets.begin(), targets.end());
    targets.erase(last, targets.end());

    Indexer indexer(this, types);

    task->work_estimated = targets.size() + 1;

    for (const auto &target : targets) {
        std::cout << "indexing " << target << std::endl;
        indexer.index(target);
        task->work_done += 1;
    }

    for (const auto *ds : indexer.finalize()) {
        task->changes.emplace_back(DbChangeType::Insert, ds->get_name());
    }

    task->work_done += 1;
}

void DatabaseSnapshot::reindex_dataset(
        Task *task, const std::vector<IndexType> &types, const std::string &dataset_name) const {
    const OnDiskDataset *source = nullptr;

    for (const auto *ds : datasets) {
        if (ds->get_id() == dataset_name) {
            source = ds;
        }
    }

    if (source == nullptr) {
        throw std::runtime_error("source dataset was not found");
    }

    db_handle.request_dataset_lock({ source->get_name() });

    Indexer indexer(this, types);

    task->work_estimated = source->indexed_files().size() + 1;

    for (const auto &target : source->indexed_files()) {
        std::cout << "reindexing " << target << std::endl;
        indexer.index(target);
        task->work_done += 1;
    }

    for (const auto *ds : indexer.finalize()) {
        task->changes.emplace_back(DbChangeType::Insert, ds->get_name());
    }

    task->changes.emplace_back(DbChangeType::Drop, source->get_name());
    task->work_done += 1;
}

std::string DatabaseSnapshot::allocate_name() const {
    while (true) {
        // TODO limit this to some sane value (like 10000 etc),
        // to avoid infinite loop in exceptional cases.

        std::stringstream ss;
        ss << "set." << random_hex_string(8) << "." << db_name.string();
        std::string fname = ss.str();
        ExclusiveFile lock(db_base / fname);
        if (lock.is_ok()) {
            return fname;
        }
    }
}

void DatabaseSnapshot::execute(const Query &query, Task *task, std::vector<std::string> *out) const {
    task->work_estimated = datasets.size();

    for (const auto &ds : datasets) {
        ds->execute(query, out);
        task->work_done += 1;
    }
}

void DatabaseSnapshot::smart_compact(Task *task) const {
    std::vector<const OnDiskDataset *> candidates = OnDiskDataset::get_compact_candidates(datasets);

    if (candidates.empty()) {
        throw std::runtime_error("no candidates for smart compact");
    }

    internal_compact(task, candidates);
}

void DatabaseSnapshot::compact(Task *task) const {
    internal_compact(task, datasets);
}

void DatabaseSnapshot::internal_compact(Task *task, std::vector<const OnDiskDataset *> datasets) const {
    std::vector<std::string> ds_names;

    for (const auto *ds : datasets) {
        ds_names.push_back(ds->get_name());
    }

    db_handle.request_dataset_lock(ds_names);

    std::string dataset_name = allocate_name();
    OnDiskDataset::merge(db_base, dataset_name, datasets, task);

    for (auto &dataset : datasets) {
        task->changes.emplace_back(DbChangeType::Drop, dataset->get_name());
    }

    task->changes.emplace_back(DbChangeType::Insert, dataset_name);
}

void DatabaseSnapshot::set_db_handle(DatabaseHandle handle) {
    db_handle = handle;
}

void DatabaseSnapshot::lock_dataset(const std::string &ds_name) {
    locked_datasets.insert(ds_name);
}

bool DatabaseSnapshot::is_locked(const std::string &ds_name) const {
    return locked_datasets.count(ds_name) > 0;
}
