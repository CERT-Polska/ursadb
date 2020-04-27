#pragma once

#include <experimental/filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "DatabaseConfig.h"
#include "DatabaseHandle.h"
#include "DatabaseName.h"
#include "OnDiskDataset.h"
#include "OnDiskIterator.h"
#include "Query.h"
#include "ResultWriter.h"
#include "Task.h"

// Represents immutable snapshot of database state.
// Should never change, regardless of changes in real database.
class DatabaseSnapshot {
    fs::path db_name;
    fs::path db_base;
    std::map<std::string, OnDiskIterator> iterators;
    DatabaseConfig config;
    std::vector<const OnDiskDataset *> datasets;
    std::set<std::string> locked_datasets;
    std::set<std::string> locked_iterators;
    std::map<uint64_t, Task> tasks;
    DatabaseHandle db_handle;

    void find_all_indexed_files(std::set<std::string> *indexed) const;
    void build_target_list(const std::string &filepath,
                           const std::set<std::string> &existing_files,
                           std::vector<std::string> *targets) const;

    friend class Indexer;

   public:
    DatabaseName allocate_name(const std::string &type = "set") const;

    DatabaseSnapshot(
        fs::path db_name, fs::path db_base, DatabaseConfig config,
        std::map<std::string, OnDiskIterator> iterators,
        std::vector<const OnDiskDataset *> datasets,
        const std::unordered_map<uint64_t, std::unique_ptr<Task>> &tasks);
    void set_db_handle(DatabaseHandle handle);

    // For use by the db coordinator from a synchronised context.
    // You probably don't want to use these methods directly - use
    // DatabaseHandle::request_dataset_lock instead.
    void lock_dataset(const std::string &ds_name);
    void lock_iterator(const std::string &it_name);
    bool is_dataset_locked(const std::string &ds_name) const;
    bool is_iterator_locked(const std::string &it_name) const;

    DatabaseName derive_name(const DatabaseName &original,
                             const std::string &type) const {
        std::string fname =
            type + "." + original.get_id() + "." + db_name.string();
        return DatabaseName(db_base, type, original.get_id(), fname);
    }

    bool read_iterator(Task *task, const std::string &iterator_id, int count,
                       std::vector<std::string> *out,
                       uint64_t *out_iterator_position,
                       uint64_t *out_iterator_files) const;

    // Recursively indexes files under paths in `filepaths`. Ensures that no
    // file will be indexed twice - this may be a very memory-heavy operation
    void recursive_index_paths(Task *task, const std::vector<IndexType> &types,
                               const std::vector<std::string> &filepaths) const;

    // Recursively indexes files under paths in `filepaths`. Does not check for
    // duplicated files, which makes if faster, but also more dangerous.
    void force_recursive_index_paths(
        Task *task, const std::vector<IndexType> &types,
        const std::vector<std::string> &filepaths) const;

    // Indexes files with given paths. Ensures that no file will be indexed
    // twice - this may be a very memory-heavy operation.
    void index_files(Task *task, const std::vector<IndexType> &types,
                     const std::vector<std::string> &filepaths) const;

    // Indexes files with given paths. Does not check for
    // duplicated files, which makes if faster, but also more dangerous.
    void force_index_files(Task *task, const std::vector<IndexType> &types,
                           const std::vector<std::string> &filepaths) const;

    void reindex_dataset(Task *task, const std::vector<IndexType> &types,
                         const std::string &dataset_name) const;
    void execute(const Query &query, const std::set<std::string> &taints,
                 Task *task, ResultWriter *out) const;
    void smart_compact(Task *task) const;
    void compact(Task *task) const;
    void internal_compact(Task *task,
                          std::vector<const OnDiskDataset *> datasets) const;
    const OnDiskDataset *find_dataset(const std::string &name) const;
    const std::vector<const OnDiskDataset *> &get_datasets() const {
        return datasets;
    };
    const std::map<uint64_t, Task> &get_tasks() const { return tasks; };

    const DatabaseConfig &get_config() const { return config; }
};
