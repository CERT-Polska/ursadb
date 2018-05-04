#pragma once

#include <experimental/filesystem>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "DatabaseSnapshot.h"
#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "Query.h"
#include "Task.h"

namespace fs = std::experimental::filesystem;

class OnDiskDataset;

class Database {
    fs::path db_name;
    fs::path db_base;
    std::vector<OnDiskDataset *> working_datasets;
    std::vector<std::unique_ptr<OnDiskDataset>> loaded_datasets;
    size_t max_memory_size;

    uint64_t last_task_id;
    std::map<uint64_t, std::unique_ptr<Task>> tasks;

    uint64_t allocate_task_id();
    void load_from_disk();

    explicit Database(const std::string &fname, bool initialize);

  public:
    explicit Database(const std::string &fname);

    const std::map<uint64_t, std::unique_ptr<Task>> &current_tasks() { return tasks; }
    void commit_task(uint64_t task_id);
    Task *get_task(uint64_t task_id);
    void erase_task(uint64_t task_id);
    Task *allocate_task();
    Task *allocate_task(const std::string &request, const std::string &conn_id);

    const std::vector<OnDiskDataset *> &working_sets() { return working_datasets; }
    const std::vector<std::unique_ptr<OnDiskDataset>> &loaded_sets() { return loaded_datasets; }

    static void create(const std::string &path);
    void load_dataset(const std::string &dsname);
    void drop_dataset(const std::string &dsname);
    void destroy_dataset(const std::string &dsname);
    void collect_garbage(std::set<DatabaseSnapshot*> &working_snapshots);
    DatabaseSnapshot snapshot();
    void save();
};
