#pragma once

#include <experimental/filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "Query.h"
#include "Task.h"
#include "Utils.h"
#include "DatabaseHandle.h"

class OnDiskDataset;

// Represents immutable snapshot of database state.
// Should never change, regardless of changes in real database.
class DatabaseSnapshot {
    fs::path db_name;
    fs::path db_base;
    std::vector<const OnDiskDataset *> datasets;
    std::set<std::string> locked_datasets;
    std::map<uint64_t, Task> tasks;
    size_t max_memory_size;

    DatabaseHandle db_handle;

    std::string allocate_name() const;
    void build_target_list(
            const std::string &filepath, const std::set<std::string> &existing_files,
            std::vector<std::string> &targets) const;

    friend class Indexer;

  public:
    DatabaseSnapshot(
            fs::path db_name, fs::path db_base, std::vector<const OnDiskDataset *> datasets,
            const std::map<uint64_t, std::unique_ptr<Task>> &tasks, size_t max_memory_size);
    void set_db_handle(DatabaseHandle handle);
    void lock_dataset(const std::string &ds_name);
    bool is_locked(const std::string &ds_name) const;

    void index_path(Task *task,
                    const std::vector<IndexType> &types, const std::vector<std::string> &filepaths) const;
    void reindex_dataset(Task *task, const std::vector<IndexType> &types, const std::string &dataset_name) const;
    void execute(const Query &query, Task *task, std::vector<std::string> *out) const;
    void smart_compact(Task *task) const;
    void compact(Task *task) const;
    void internal_compact(Task *task, std::vector<const OnDiskDataset *> datasets) const;
    const std::vector<const OnDiskDataset *> &get_datasets() const { return datasets; };
    const std::map<uint64_t, Task> &get_tasks() const { return tasks; };
};
