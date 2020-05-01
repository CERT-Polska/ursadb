#pragma once

#include <experimental/filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "DatabaseConfig.h"
#include "DatabaseName.h"
#include "OnDiskDataset.h"
#include "OnDiskIterator.h"
#include "Query.h"
#include "ResultWriter.h"
#include "Task.h"

// Represents immutable snapshot of database state.
// Should never change, regardless of changes in the real database.
class DatabaseSnapshot {
    fs::path db_name;
    fs::path db_base;
    std::map<std::string, OnDiskIterator> iterators;
    DatabaseConfig config;
    std::vector<const OnDiskDataset *> datasets;
    std::set<std::string> locked_datasets;
    std::set<std::string> locked_iterators;
    std::unordered_map<uint64_t, TaskSpec> tasks;

    void find_all_indexed_files(std::set<std::string> *indexed) const;
    void build_target_list(const std::string &filepath,
                           const std::set<std::string> &existing_files,
                           std::vector<std::string> *targets) const;

    friend class Indexer;

    void internal_compact(Task *task,
                          std::vector<const OnDiskDataset *> datasets) const;

    // Internal function used to find both full and smart candidates.
    std::vector<std::string> find_compact_candidate(bool smart) const;

   public:
    DatabaseName allocate_name(const std::string &type = "set") const;

    DatabaseSnapshot(fs::path db_name, fs::path db_base, DatabaseConfig config,
                     std::map<std::string, OnDiskIterator> iterators,
                     std::vector<const OnDiskDataset *> datasets,
                     const std::unordered_map<uint64_t, TaskSpec> &tasks);

    DatabaseName derive_name(const DatabaseName &original,
                             const std::string &type) const {
        std::string fname =
            type + "." + original.get_id() + "." + db_name.string();
        return DatabaseName(db_base, type, original.get_id(), fname);
    }

    void read_iterator(Task *task, const std::string &iterator_id, int count,
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
    QueryStatistics execute(const Query &query,
                            const std::set<std::string> &taints,
                            const std::set<std::string> &datasets, Task *task,
                            ResultWriter *out) const;

    // Find candidates for compacting, but in a "smart" way - if the algorithm
    // decides that there are no good candidates, it won't do anything.
    std::vector<std::string> compact_smart_candidates() const;

    // Find candidates for compacting. If there are at least two datasets that
    // can be merged, it's guaranteed that they will be. This function still
    // tries to find best candidates to merge
    std::vector<std::string> compact_full_candidates() const;

    void compact_locked_datasets(Task *task) const;

    const OnDiskDataset *find_dataset(const std::string &name) const;
    const std::vector<const OnDiskDataset *> &get_datasets() const {
        return datasets;
    };
    const std::unordered_map<uint64_t, TaskSpec> &get_tasks() const {
        return tasks;
    };

    const DatabaseConfig &get_config() const { return config; }
};
