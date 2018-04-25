#pragma once

#include <string>
#include <vector>
#include <experimental/filesystem>

#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "Query.h"

namespace fs = std::experimental::filesystem;


class Database {
    fs::path db_name;
    fs::path db_base;
    int num_datasets;
    std::set<std::string> all_files;
    std::vector<OnDiskDataset> datasets;
    std::string allocate_name();
    size_t max_memory_size;

    void set_filename(const std::string &fname);

  public:
    explicit Database(const std::string &fname);
    explicit Database();
    void index_path(const std::vector<IndexType> types, const std::string &filepath);
    void execute(const Query &query, std::vector<std::string> &out);
    void add_dataset(DatasetBuilder &builder);
    void compact();
    void save();

    static void create(const std::string &path);
};
