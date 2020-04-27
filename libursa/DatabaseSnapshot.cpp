#include "DatabaseSnapshot.h"

#include <fstream>
#include <stdexcept>

#include "ExclusiveFile.h"
#include "Indexer.h"
#include "Utils.h"
#include "spdlog/spdlog.h"

DatabaseSnapshot::DatabaseSnapshot(
    fs::path db_name, fs::path db_base,
    std::map<std::string, OnDiskIterator> iterators,
    std::vector<const OnDiskDataset *> datasets,
    const std::unordered_map<uint64_t, std::unique_ptr<Task>> &tasks)
    : db_name(db_name),
      db_base(db_base),
      iterators(iterators),
      datasets(datasets),
      tasks() {
    for (const auto &entry : tasks) {
        this->tasks.emplace(entry.first, *entry.second.get());
    }
}

const OnDiskDataset *DatabaseSnapshot::find_dataset(
    const std::string &name) const {
    for (const auto &ds : datasets) {
        if (ds->get_id() == name) {
            return ds;
        }
    }
    return nullptr;
}

bool DatabaseSnapshot::read_iterator(Task *task, const std::string &iterator_id,
                                     int count, std::vector<std::string> *out,
                                     uint64_t *out_iterator_position,
                                     uint64_t *out_iterator_files) const {
    *out_iterator_files = 0;
    *out_iterator_position = 0;

    auto it = iterators.find(iterator_id);
    if (it == iterators.end()) {
        throw std::runtime_error("tried to read non-existent iterator");
    }

    // TODO maybe should busy-wait on failure?
    if (!db_handle.request_iterator_lock(iterator_id)) {
        return false;
    }

    OnDiskIterator iterator_copy = it->second;
    iterator_copy.pop(count, out);
    *out_iterator_files = iterator_copy.get_total_files();
    *out_iterator_position = iterator_copy.get_file_offset();

    std::string byte_offset = std::to_string(iterator_copy.get_byte_offset());
    std::string file_offset = std::to_string(iterator_copy.get_file_offset());
    std::string param = byte_offset + ":" + file_offset;
    task->changes.emplace_back(DbChangeType::UpdateIterator,
                               iterator_copy.get_name().get_filename(), param);
    return true;
}

void DatabaseSnapshot::build_target_list(
    const std::string &filepath, const std::set<std::string> &existing_files,
    std::vector<std::string> *targets) const {
    fs::recursive_directory_iterator end;

    if (fs::is_regular_file(filepath)) {
        fs::path absfn = fs::absolute(filepath);

        if (existing_files.count(absfn) == 0) {
            targets->push_back(absfn);
        }
    } else {
        for (fs::recursive_directory_iterator dir(filepath); dir != end;
             ++dir) {
            if (fs::is_regular_file(dir->path())) {
                fs::path absfn = fs::absolute(dir->path());
                if (existing_files.count(absfn) == 0) {
                    targets->push_back(absfn);
                }
            }
        }
    }
}

void DatabaseSnapshot::find_all_indexed_files(
    std::set<std::string> *existing_files) const {
    for (const auto &ds : datasets) {
        ds->for_each_filename([&existing_files](const std::string &fname) {
            existing_files->insert(fname);
        });
    }
}

void DatabaseSnapshot::recursive_index_paths(
    Task *task, const std::vector<IndexType> &types,
    const std::vector<std::string> &root_paths) const {
    std::vector<std::string> targets;
    {
        std::set<std::string> existing_files;
        find_all_indexed_files(&existing_files);
        for (const auto &filepath : root_paths) {
            build_target_list(filepath, existing_files, &targets);
        }
    }
    force_index_files(task, types, targets);
}

void DatabaseSnapshot::force_recursive_index_paths(
    Task *task, const std::vector<IndexType> &types,
    const std::vector<std::string> &root_paths) const {
    std::vector<std::string> targets;
    for (const auto &filepath : root_paths) {
        build_target_list(filepath, {}, &targets);
    }
    force_index_files(task, types, targets);
}

void DatabaseSnapshot::index_files(
    Task *task, const std::vector<IndexType> &types,
    const std::vector<std::string> &filenames) const {
    std::vector<std::string> unique_filenames;
    {
        std::set<std::string> existing_files;
        find_all_indexed_files(&existing_files);
        for (const auto &filename : filenames) {
            if (existing_files.count(filename) == 0) {
                unique_filenames.push_back(filename);
            }
        }
    }
    force_index_files(task, types, unique_filenames);
}

void DatabaseSnapshot::force_index_files(
    Task *task, const std::vector<IndexType> &types,
    const std::vector<std::string> &targets) const {
    if (targets.empty()) {
        return;
    }

    Indexer indexer(this, types);

    task->work_estimated = targets.size() + 1;

    for (const auto &target : targets) {
        spdlog::debug("Indexing {}", target);
        indexer.index(target);
        task->work_done += 1;
    }

    for (const auto *ds : indexer.finalize()) {
        task->changes.emplace_back(DbChangeType::Insert, ds->get_name());
    }

    task->work_done += 1;
}

void DatabaseSnapshot::reindex_dataset(Task *task,
                                       const std::vector<IndexType> &types,
                                       const std::string &dataset_name) const {
    const OnDiskDataset *source = nullptr;

    for (const auto *ds : datasets) {
        if (ds->get_id() == dataset_name) {
            source = ds;
        }
    }

    if (source == nullptr) {
        throw std::runtime_error("source dataset was not found");
    }

    if (!db_handle.request_dataset_lock({source->get_name()})) {
        throw std::runtime_error("can't lock the dataset - try again later");
    }

    Indexer indexer(this, types);

    task->work_estimated = source->get_file_count() + 1;

    source->for_each_filename([&indexer, &task](const std::string &target) {
        spdlog::debug("Reindexing {}", target);
        indexer.index(target);
        task->work_done += 1;
    });

    for (const auto *ds : indexer.finalize()) {
        task->changes.emplace_back(DbChangeType::Insert, ds->get_name());
    }

    task->changes.emplace_back(DbChangeType::Drop, source->get_name());
    task->work_done += 1;
}

DatabaseName DatabaseSnapshot::allocate_name(const std::string &type) const {
    while (true) {
        // TODO limit this to some sane value (like 10000 etc),
        // to avoid infinite loop in exceptional cases.

        std::stringstream ss;
        std::string id(random_hex_string(8));
        ss << type << "." << id << "." << db_name.string();
        std::string fname = ss.str();
        ExclusiveFile lock(db_base / fname);
        if (lock.is_ok()) {
            return DatabaseName(db_base, type, id, fname);
        }
    }
}

void DatabaseSnapshot::execute(const Query &query,
                               const std::set<std::string> &taints, Task *task,
                               ResultWriter *out) const {
    task->work_estimated = datasets.size();

    for (const auto &ds : datasets) {
        if (!ds->has_all_taints(taints)) {
            continue;
        }
        ds->execute(query, out);
        task->work_done += 1;
    }
}

void DatabaseSnapshot::smart_compact(Task *task) const {
    for (auto set : OnDiskDataset::get_taint_compatible_datasets(datasets)) {
        std::vector<const OnDiskDataset *> candidates =
            OnDiskDataset::get_compact_candidates(set);

        if (!candidates.empty()) {
            internal_compact(task, candidates);
        }
    }
}

void DatabaseSnapshot::compact(Task *task) const {
    for (auto set : OnDiskDataset::get_taint_compatible_datasets(datasets)) {
        if (set.size() >= 2) {
            internal_compact(task, set);
        }
    }
}

void DatabaseSnapshot::internal_compact(
    Task *task, std::vector<const OnDiskDataset *> datasets) const {
    std::vector<std::string> ds_names;

    for (const auto *ds : datasets) {
        ds_names.push_back(ds->get_name());
    }

    if (!db_handle.request_dataset_lock(ds_names)) {
        throw std::runtime_error("can't lock the datasets - try again later");
    }

    std::string dataset_name = allocate_name().get_filename();
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

void DatabaseSnapshot::lock_iterator(const std::string &it_name) {
    locked_iterators.insert(it_name);
}

bool DatabaseSnapshot::is_dataset_locked(const std::string &ds_name) const {
    return locked_datasets.count(ds_name) > 0;
}

bool DatabaseSnapshot::is_iterator_locked(const std::string &it_name) const {
    return locked_iterators.count(it_name) > 0;
}
