#pragma once

#include <string>
#include <vector>

#include "Core.h"
#include "OnDiskIndex.h"
#include "Query.h"
#include "Task.h"

class OnDiskIndex;

class OnDiskDataset {
    std::string name;
    fs::path db_base;
    std::string files_fname;
    // set of indexed filenames ordered with respect to FileId
    std::vector<std::string> fnames;
    std::vector<OnDiskIndex> indices;

    const std::string &get_file_name(FileId fid) const;
    QueryResult query_str(const QString &str) const;
    QueryResult internal_execute(const Query &query) const;
    const OnDiskIndex &get_index_with_type(IndexType index_type) const;
    void drop_file(const std::string &fname) const;
    QueryResult pick_common(int cutoff, const std::vector<Query> &queries) const;

  public:
    explicit OnDiskDataset(const fs::path &db_base, const std::string &fname);
    const std::string &get_name() const;
    fs::path get_base() const;
    const std::vector<std::string> &indexed_files() const { return fnames; }
    void execute(const Query &query, std::vector<std::string> *out) const;
    static void
    merge(const fs::path &db_base, const std::string &outname,
          const std::vector<const OnDiskDataset *> &datasets, Task *task);
    void drop();
    std::string get_id() const;
    const std::vector<OnDiskIndex> &get_indexes() const { return indices; }
    static std::vector<const OnDiskDataset *> get_compact_candidates(const std::vector<const OnDiskDataset *> &datasets);
};
