#pragma once

#include <experimental/filesystem>
#include <vector>

#include "Core.h"
#include "IndexBuilder.h"
#include "Utils.h"

class DatasetBuilder {
  public:
    DatasetBuilder(const std::vector<IndexType> &index_types);

    void index(const std::string &filepath);
    void save(const fs::path &db_base, const std::string &fname);
    bool must_spill();
    bool empty() const { return fids.empty(); }

  private:
    std::vector<std::string> fids;
    std::vector<IndexBuilder> indices;

    FileId register_fname(const std::string &fname);
};
