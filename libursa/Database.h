#pragma once

#include <experimental/filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "DatabaseConfig.h"
#include "DatabaseSnapshot.h"
#include "OnDiskDataset.h"
#include "OnDiskIterator.h"
#include "Task.h"

class Database {
    fs::path db_name;
    fs::path db_base;
    std::map<std::string, OnDiskIterator> iterators;
    std::vector<OnDiskDataset *> working_datasets;
    std::vector<std::unique_ptr<OnDiskDataset>> loaded_datasets;
    DatabaseConfig config_;

    uint64_t last_task_id;
    std::unordered_map<uint64_t, std::unique_ptr<TaskSpec>> tasks;

    uint64_t allocate_task_id();
    void load_from_disk();

    explicit Database(const std::string &fname, bool initialize);

    bool can_acquire(const DatabaseLock &newlock) const;

   public:
    explicit Database(const std::string &fname);

    const fs::path &get_name() const { return db_name; };
    const fs::path &get_base() const { return db_base; };
    const std::unordered_map<uint64_t, std::unique_ptr<TaskSpec>>
        &current_tasks() {
        return tasks;
    }
    void commit_task(const TaskSpec &spec,
                     const std::vector<DBChange> &changes);
    const TaskSpec &get_task(uint64_t task_id);
    void erase_task(uint64_t task_id);
    TaskSpec *allocate_task();
    TaskSpec *allocate_task(const std::string &request,
                            const std::string &conn_id,
                            const std::vector<DatabaseLock> &locks);

    const std::vector<OnDiskDataset *> &working_sets() const {
        return working_datasets;
    }
    const std::vector<std::unique_ptr<OnDiskDataset>> &loaded_sets() const {
        return loaded_datasets;
    }

    const std::map<std::string, OnDiskIterator> &get_iterators() const {
        return iterators;
    }

    static void create(const std::string &fname);
    void load_iterator(const DatabaseName &name);
    void update_iterator(const DatabaseName &name, uint64_t byte_offset,
                         uint64_t file_offset);
    void load_dataset(const std::string &ds);
    OnDiskDataset *find_working_dataset(const std::string &name);
    void drop_dataset(const std::string &dsname);
    void destroy_dataset(const std::string &dsname);
    void collect_garbage(std::set<DatabaseSnapshot *> &working_snapshots);
    DatabaseSnapshot snapshot();
    void save();
    const DatabaseConfig &config() { return config_; }
};
