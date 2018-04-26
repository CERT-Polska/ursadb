#pragma once

#include <experimental/filesystem>
#include <string>
#include <vector>

#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "Query.h"

namespace fs = std::experimental::filesystem;

class Task {
public:
    uint64_t id; // unique task id
    uint64_t work_estimated; // arbitrary number <= work_done
    uint64_t work_done; // arbitrary number, for example "number of bytes to index" or "number of trigrams to merge"
    uint64_t start_timestamp; // for ETA calculation
};

class Database {
    fs::path db_name;
    fs::path db_base;
    int num_datasets;
    std::set<std::string> all_files;
    std::vector<OnDiskDataset> datasets;
    std::vector<Task> tasks;
    std::string allocate_name();
    size_t max_memory_size;

    void set_filename(const std::string &fname);

  public:
    explicit Database(const std::string &fname);
    explicit Database();
    void index_path(const std::vector<IndexType> types, const std::string &filepath);
    void execute(const Query &query, std::vector<std::string> &out);
    void add_dataset(DatasetBuilder &builder);
    const std::vector<Task> &current_tasks() { return tasks; }
    void compact();
    void save();

    static void create(const std::string &path);
};
