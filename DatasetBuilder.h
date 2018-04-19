#pragma once

#include <array>
#include <fstream>
#include <set>
#include <vector>

#include "Core.h"
#include "IndexBuilder.h"

class DatasetBuilder {
  public:
    DatasetBuilder(const std::vector<IndexType> &index_types);

    void index(const std::string &filepath);
    void save(const std::string &fname);
    size_t real_size() const;

  private:
    std::vector<std::string> fids;
    std::vector<IndexBuilder> indices;
    long total_bytes;

    FileId register_fname(const std::string &fname);
};
