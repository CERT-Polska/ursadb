#pragma once

#include <string>
#include <vector>

#include "Core.h"
#include "OnDiskIndex.h"
#include "Query.h"

class OnDiskDataset {
    std::string name;
    std::string filename_list;
    std::vector<std::string> fnames;
    std::vector<OnDiskIndex> indices;

    const std::string &get_file_name(FileId fid) const;
    QueryResult query_str(const std::string &str) const;
    QueryResult internal_execute(const Query &query) const;
    const OnDiskIndex &get_index_with_type(IndexType index_type) const;

  public:
    explicit OnDiskDataset(const std::string &fname);
    const std::string &get_name() const;
    void execute(const Query &query, std::vector<std::string> *out) const;
    static void merge(const std::string &outname, const std::vector<OnDiskDataset> &datasets);
    void drop();
};
