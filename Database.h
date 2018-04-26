#pragma once

#include <experimental/filesystem>
#include <map>
#include <string>
#include <vector>

#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "Query.h"
#include "Task.h"

namespace fs = std::experimental::filesystem;

class OnDiskDataset;

class Database {
    fs::path db_name;
    fs::path db_base;
    int num_datasets;
    std::set<std::string> all_files;
    std::vector<OnDiskDataset> datasets;
    std::string allocate_name();
    size_t max_memory_size;

    uint64_t last_task_id;
    std::vector<Task> tasks;
    uint64_t allocate_task_id() {
        last_task_id++;
        return last_task_id;
    }
    void set_filename(const std::string &fname);

  public:
    explicit Database(const std::string &fname);
    explicit Database();
    void index_path(Task *task, const std::vector<IndexType> types, const std::string &filepath);
    void execute(const Query &query, Task *task, std::vector<std::string> *out);
    void add_dataset(DatasetBuilder &builder);
    const std::vector<Task> &current_tasks() { return tasks; }
    void compact(Task *task);
    void save();

    static void create(const std::string &path);
    Task *allocate_task() {
        uint64_t task_id = allocate_task_id();
        return &tasks.emplace_back(task_id);
    }
};
