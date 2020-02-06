#pragma once

#include <string>
#include <vector>
#include <array>

#include "Core.h"
#include "IndexBuilder.h"

class FlatIndexBuilder : public IndexBuilder {
    // Contains uint64_t values in the following format:
    // [trigram: 24 bits] [file_id: 40 bits]
    std::vector<uint64_t> raw_data;

    void add_trigram(FileId fid, TriGram val);

  public:
    FlatIndexBuilder(IndexType ntype);
    virtual ~FlatIndexBuilder() = default;

    void add_file(FileId fid, const uint8_t *data, size_t size);
    void save(const std::string &fname);
    bool can_still_add(uint64_t bytes, int file_count) const;
};
