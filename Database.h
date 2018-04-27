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

namespace fs = std::experimental::filesystem;

class OnDiskDataset;

class DatabaseSnapshot {
    fs::path db_name;
    fs::path db_base;
    std::vector<const OnDiskDataset*> datasets;
    size_t max_memory_size;
    std::mt19937_64 random;

    std::string allocate_name();

public:
    DatabaseSnapshot(fs::path db_name, fs::path db_base, std::vector<const OnDiskDataset*> datasets, size_t max_memory_size)
            : db_name(db_name), db_base(db_base), datasets(datasets), max_memory_size(max_memory_size) {}
    void index_path(Task *task, const std::vector<IndexType> types, const std::string &filepath);
    void execute(const Query &query, Task *task, std::vector<std::string> *out);
    void compact(Task *task);
};

class Database {
    fs::path db_name;
    fs::path db_base;
    std::vector<OnDiskDataset> working_datasets;
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
    const std::vector<OnDiskDataset> &datasets() { return working_datasets; }

    static void create(const std::string &path);
    void load_dataset(const std::string &dsname);
    void drop_dataset(const std::string &dsname);
    DatabaseSnapshot snapshot() {
        std::vector<const OnDiskDataset*> cds;

        for (const auto &d : working_datasets) {
            cds.push_back(&d);
        }

        return DatabaseSnapshot(db_name, db_base, cds, max_memory_size);
    }
};
