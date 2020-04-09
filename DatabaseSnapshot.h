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
#include "ResultWriter.h"

class OnDiskDataset;
class OnDiskIterator;

// Represents a name in the database. Usually a filename of the form
// "type.id.dbname", for exmaple "set.23381d1f.db.ursa".
// In case of manual rename, filename may not match this format.
class DatabaseName {
    fs::path db_base;
    std::string type;
    std::string id;
    std::string filename;

public:
    DatabaseName(
        fs::path db_base,
        std::string type,
        std::string id,
        std::string filename
    ): db_base(db_base), type(type), id(id), filename(filename) {}

    DatabaseName derive(
        const std::string &new_type,
        const std::string &new_filename
    ) const;

    DatabaseName derive_temporary() const;

    std::string get_id() const {
        return id;
    }

    fs::path get_full_path() const {
        return db_base / filename;
    }

    std::string get_filename() const {
        return filename;
    }

    // TODO: this method should not exist, convert all legacy name occurences
    // to DatbaseName.
    static DatabaseName parse(fs::path db_base, std::string name);
};

// Represents immutable snapshot of database state.
// Should never change, regardless of changes in real database.
class DatabaseSnapshot {
    fs::path db_name;
    fs::path db_base;
    std::map<std::string, OnDiskIterator> iterators;
    std::vector<const OnDiskDataset *> datasets;
    std::set<std::string> locked_datasets;
    std::set<std::string> locked_iterators;
    std::map<uint64_t, Task> tasks;
    size_t max_memory_size;
    DatabaseHandle db_handle;

    void build_new_target_list(
        const std::vector<std::string> &filepaths,
        std::vector<std::string> *targets
    ) const;
    void build_target_list(
        const std::string &filepath,
        const std::set<std::string> &existing_files,
        std::vector<std::string> *targets
    ) const;

    friend class Indexer;

  public:
    DatabaseName allocate_name(const std::string &type="set") const;

    DatabaseSnapshot(
        fs::path db_name,
        fs::path db_base,
        std::map<std::string, OnDiskIterator> iterators,
        std::vector<const OnDiskDataset *> datasets,
        const std::map<uint64_t, std::unique_ptr<Task>> &tasks,
        size_t max_memory_size
    );
    void set_db_handle(DatabaseHandle handle);

    // For use by the db coordinator from a synchronised context.
    // You probably don't want to use these methods directly - use
    // DatabaseHandle::request_dataset_lock instead.
    void lock_dataset(const std::string &ds_name);
    void lock_iterator(const std::string &it_name);
    bool is_dataset_locked(const std::string &ds_name) const;
    bool is_iterator_locked(const std::string &it_name) const;

    DatabaseName derive_name(
        const DatabaseName &original,
        const std::string &type
    ) const {
        std::string fname = type + "." + original.get_id() + "." + db_name.string();
        return DatabaseName(db_base, type, original.get_id(), fname);
    }

    bool read_iterator(
        Task *task,
        const std::string &iterator_id,
        int count,
        std::vector<std::string> *out,
        uint64_t *out_iterator_position,
        uint64_t *out_iterator_files
    ) const;
    void index_path(
        Task *task,
        const std::vector<IndexType> &types,
        const std::vector<std::string> &filepaths
    ) const;
    void reindex_dataset(
        Task *task,
        const std::vector<IndexType> &types,
        const std::string &dataset_name
    ) const;
    void execute(
        const Query &query,
        const std::set<std::string> &taints,
        Task *task,
        ResultWriter *out
    ) const;
    void smart_compact(Task *task) const;
    void compact(Task *task) const;
    void internal_compact(
        Task *task,
        std::vector<const OnDiskDataset *> datasets
    ) const;
    const OnDiskDataset *find_dataset(const std::string &name) const;
    const std::vector<const OnDiskDataset *> &get_datasets() const {
        return datasets;
    };
    const std::map<uint64_t, Task> &get_tasks() const { return tasks; };
};
