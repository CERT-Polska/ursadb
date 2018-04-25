#pragma once

#include <experimental/filesystem>
#include <vector>

#include "Core.h"
#include "IndexBuilder.h"

namespace fs = std::experimental::filesystem;

class DatasetBuilder {
  public:
    DatasetBuilder(const std::vector<IndexType> &index_types);

    void index(const std::string &filepath);
    void save(const fs::path &db_base, const std::string &fname);
    uint64_t estimated_size();
    bool empty() const { return fids.empty(); }

  private:
    std::vector<std::string> fids;
    std::vector<IndexBuilder> indices;
    uint64_t total_bytes;

    FileId register_fname(const std::string &fname);
};
