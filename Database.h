#pragma once

#include <experimental/filesystem>
#include <map>
#include <string>
#include <vector>
#include <random>

#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "Query.h"
#include "Task.h"

namespace fs = std::experimental::filesystem;

class OnDiskDataset;

class Database {
    fs::path db_name;
    fs::path db_base;
    std::set<std::string> all_files;
    std::vector<OnDiskDataset> datasets;
    size_t max_memory_size;
    uint64_t last_task_id;
    std::map<uint64_t, Task> tasks;
    std::mt19937_64 random;

    std::string allocate_name();
    uint64_t allocate_task_id();
    void load_from_disk();

    explicit Database(const std::string &fname, bool initialize);

  public:
    explicit Database(const std::string &fname);
    void index_path(Task *task, const std::vector<IndexType> types, const std::string &filepath);
    void execute(const Query &query, Task *task, std::vector<std::string> *out);
    void add_dataset(DatasetBuilder &builder);
    const std::map<uint64_t, Task> &current_tasks() { return tasks; }
    void compact(Task *task);
    void save();
    Task *allocate_task();
    const std::vector<OnDiskDataset> &get_datasets() { return datasets; }

    static void create(const std::string &path);
};
