#include "DatabaseSnapshot.h"

#include <fstream>

#include "Database.h"
#include "DatasetBuilder.h"
#include "ExclusiveFile.h"
#include "Json.h"
#include "Indexer.h"

std::string random_hex_string(int length) {
    constexpr static char charset[] = "0123456789abcdef";
    thread_local static std::random_device rd;
    thread_local static std::seed_seq seed{rd(), rd(), rd(), rd()}; // A bit better than pathetic default
    thread_local static std::mt19937_64 random(seed);
    thread_local static std::uniform_int_distribution<int> pick(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);

    for (int i = 0; i < length; i++) {
        result += charset[pick(random)];
    }

    return result;
}

DatabaseSnapshot::DatabaseSnapshot(
        fs::path db_name, fs::path db_base, std::vector<const OnDiskDataset *> datasets,
        const std::map<uint64_t, std::unique_ptr<Task>> &tasks, size_t max_memory_size)
    : db_name(db_name), db_base(db_base), datasets(datasets), tasks(),
      max_memory_size(max_memory_size) {
    for (const auto &entry : tasks) {
        this->tasks.emplace(entry.first, *entry.second.get());
    }
}

std::vector<std::string> DatabaseSnapshot::build_target_list(const std::string &filepath) const {
    std::set<std::string> all_files;
    fs::recursive_directory_iterator end;

    for (auto &dataset : datasets) {
        for (auto &fn : dataset->indexed_files()) {
            all_files.insert(fn);
        }
    }

    std::vector<std::string> targets;

    for (fs::recursive_directory_iterator dir(filepath); dir != end; ++dir) {
        if (fs::is_regular_file(dir->path())) {
            fs::path absfn = fs::absolute(dir->path());

            if (all_files.find(absfn.string()) != all_files.end()) {
                continue;
            }

            targets.push_back(absfn.string());
        }
    }

    return targets;
}

void DatabaseSnapshot::index_path(
        Task *task, const std::vector<IndexType> &types, const std::string &filepath) const {
    std::vector<std::string> targets = build_target_list(filepath);
    Indexer indexer(MergeStrategy::Smart, this, types);

    task->work_estimated = targets.size() + 1;

    for (const auto &target : targets) {
        std::cout << "indexing " << target << std::endl;
        indexer.index(target);
        task->work_done += 1;
    }

    OnDiskDataset *ds = indexer.force_compact();
    task->changes.emplace_back(DbChangeType::Insert, ds->get_name());

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

    db_handle.request_dataset_lock(source->get_name());

    Indexer indexer(MergeStrategy::InOrder, this, types);

    task->work_estimated = source->indexed_files().size() + 1;

    for (const std::string &fname : source->indexed_files()) {
        indexer.index(fname);
        task->work_done += 1;
    }

    OnDiskDataset *outcome = indexer.force_compact();

    if (outcome->indexed_files() != source->indexed_files()) {
        throw std::runtime_error("reindex produced faulty dataset, file list doesn\'t match with the source");
    }

    std::ifstream in(db_base / source->get_name(), std::ifstream::binary);
    json j;
    in >> j;
    in.close();

    for (const auto &type : types) {
        fs::path old_index_name = db_base / (get_index_type_name(type) + "." + outcome->get_name());
        fs::path new_index_name = db_base / (get_index_type_name(type) + "." + source->get_name());
        fs::rename(old_index_name, new_index_name);
        j["indices"].emplace_back(get_index_type_name(type) + "." + source->get_name());
    }

    std::ofstream out(db_base / source->get_name(), std::ofstream::binary);
    out << std::setw(4) << j;
    out.close();

    fs::remove(db_base / ("files." + outcome->get_name()));
    fs::remove(db_base / outcome->get_name());

    task->changes.emplace_back(DbChangeType::Reload, source->get_name());

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
    for (const auto *ds : datasets) {
        db_handle.request_dataset_lock(ds->get_name());
    }

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

bool DatabaseSnapshot::is_locked(const std::string &ds_name) {
    return locked_datasets.find(ds_name) != locked_datasets.end();
}
