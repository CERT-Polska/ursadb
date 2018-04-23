#pragma once

#include <vector>

#include "Core.h"
#include "IndexBuilder.h"

class DatasetBuilder {
  public:
    DatasetBuilder(const std::vector<IndexType> &index_types);

    void index(const std::string &filepath);
    void save(const std::string &fname);
    uint64_t estimated_size();

  private:
    std::vector<std::string> fids;
    std::vector<IndexBuilder> indices;
    uint64_t total_bytes;

    FileId register_fname(const std::string &fname);
};
