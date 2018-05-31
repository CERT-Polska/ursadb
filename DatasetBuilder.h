#pragma once

#include <experimental/filesystem>
#include <vector>

#include "Core.h"
#include "FlatIndexBuilder.h"
#include "Utils.h"

class DatasetBuilder {
  public:
    DatasetBuilder(const std::vector<IndexType> &index_types);
    DatasetBuilder(BuilderType builderType, const std::vector<IndexType> &index_types);

    void index(const std::string &filepath);
    void save(const fs::path &db_base, const std::string &fname);
    bool must_spill();
    bool empty() const { return fids.empty(); }

  private:
    std::vector<std::string> fids;
    std::vector<std::unique_ptr<IndexBuilder>> indices;

    FileId register_fname(const std::string &fname);
};

class invalid_filename_error : public std::runtime_error {
    std::string what_message;

public:
    explicit invalid_filename_error(const std::string &__arg) : runtime_error(__arg) {}

    const char *what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_USE_NOEXCEPT;
};
