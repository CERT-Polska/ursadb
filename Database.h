#pragma once

#include <experimental/filesystem>
#include <map>
#include <string>
#include <vector>
#include <random>
#include <iostream>

#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "Query.h"
#include "Task.h"
#include "DatabaseSnapshot.h"

namespace fs = std::experimental::filesystem;

class OnDiskDataset;

class Database {
    fs::path db_name;
    fs::path db_base;
    std::vector<OnDiskDataset*> working_datasets;
    std::vector<std::unique_ptr<OnDiskDataset>> loaded_datasets;
    size_t max_memory_size;

    uint64_t last_task_id;
    std::map<uint64_t, Task> tasks;

    uint64_t allocate_task_id();
    void load_from_disk();

    explicit Database(const std::string &fname, bool initialize);

  public:
    explicit Database(const std::string &fname);

    std::map<uint64_t, Task> &current_tasks() { return tasks; }

    void save();
    Task *allocate_task();
    const std::vector<OnDiskDataset*> &working_sets() { return working_datasets; }
    const std::vector<std::unique_ptr<OnDiskDataset>> &loaded_sets() { return loaded_datasets; }

    static void create(const std::string &path);
    void load_dataset(const std::string &dsname);
    void drop_dataset(const std::string &dsname);
    DatabaseSnapshot snapshot();
};
