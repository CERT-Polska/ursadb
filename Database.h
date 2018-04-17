#pragma once

#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "Query.h"

class Database {
    std::string db_fname;
    int num_datasets;
    std::vector<OnDiskDataset> datasets;
    std::string allocate_name();

  public:
    explicit Database(const std::string &fname);
    void index_path(const std::vector<IndexType> types, const std::string &filepath);
    void execute(const Query &query, std::vector<std::string> &out);
    void add_dataset(DatasetBuilder &builder);
    void compact();
    void save();
};
