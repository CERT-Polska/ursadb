#pragma once

#include <string>
#include <vector>

#include "Task.h"
#include "Core.h"
#include "OnDiskIndex.h"
#include "Query.h"

class OnDiskIndex;

class OnDiskDataset {
    std::string name;
    fs::path db_base;
    std::string files_fname;
    std::vector<std::string> fnames;
    std::vector<OnDiskIndex> indices;

    const std::string &get_file_name(FileId fid) const;
    QueryResult query_str(const std::string &str) const;
    QueryResult internal_execute(const Query &query) const;
    const OnDiskIndex &get_index_with_type(IndexType index_type) const;
    void drop_file(const std::string &fname) const;

  public:
    explicit OnDiskDataset(const fs::path &db_base, const std::string &fname);
    const std::string &get_name() const;
    const std::vector<std::string> &indexed_files() const { return fnames; }
    void execute(const Query &query, std::vector<std::string> *out) const;
    static void
    merge(Task &task,
          const fs::path &db_base, const std::string &outname,
          const std::vector<OnDiskDataset> &datasets);
    void drop();
};
