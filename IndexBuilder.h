#pragma once

#include <string>
#include <vector>

#include "Core.h"

class IndexBuilder {
    std::vector<std::vector<FileId>> raw_index;
    IndexType ntype;
    uint64_t consumed_bytes;

    inline void add_trigram(FileId fid, TriGram val);

  public:
    IndexBuilder(IndexType ntype);

    IndexType index_type() const { return ntype; }
    void add_file(FileId fid, const uint8_t *data, size_t size);
    void save(const std::string &fname);
    uint64_t estimated_size() const { return consumed_bytes; }
};
