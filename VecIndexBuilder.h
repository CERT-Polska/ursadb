#pragma once

#include <string>
#include <vector>
#include <array>

#include "Core.h"

class VecIndexBuilder {
    std::array<bool, NUM_TRIGRAMS> added_trigrams;
    std::vector<uint32_t> raw_data;
    IndexType ntype;

    void add_trigram(FileId fid, TriGram val);

  public:
    VecIndexBuilder(IndexType ntype);

    IndexType index_type() const { return ntype; }
    void add_file(FileId fid, const uint8_t *data, size_t size);
    void save(const std::string &fname);
    bool must_spill(int file_count) const;
};
