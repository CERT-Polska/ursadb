#pragma once

#include <experimental/filesystem>
#include <vector>

#include "Core.h"
#include "IndexBuilder.h"

// TODO get rid of this define
namespace fs = std::experimental::filesystem;

class DatasetBuilder {
   public:
    DatasetBuilder(BuilderType builder_type,
                   std::vector<IndexType> index_types);

    void index(const std::string &filepath);
    void force_registered(const std::string &filepath);
    void save(const fs::path &db_base, const std::string &fname);
    bool can_still_add(uint64_t bytes) const;
    bool empty() const { return fids.empty(); }
    void clear();

   private:
    BuilderType builder_type;
    std::vector<IndexType> index_types;

    std::vector<std::string> fids;
    std::vector<std::unique_ptr<IndexBuilder>> indices;

    FileId register_fname(const std::string &fname);
};

class invalid_filename_error : public std::runtime_error {
    std::string what_message;

   public:
    explicit invalid_filename_error(const std::string &__arg)
        : runtime_error(__arg) {}

    const char *what() const noexcept;
};
