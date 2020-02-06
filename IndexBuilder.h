#pragma once

#include <string>
#include <vector>

#include "Core.h"

class IndexBuilder {
    IndexType ntype;

  public:
    IndexBuilder(IndexType ntype) : ntype(ntype) {}
    virtual ~IndexBuilder() = default;

    IndexType index_type() const { return ntype; }
    virtual void add_file(FileId fid, const uint8_t *data, size_t size) = 0;
    virtual void save(const std::string &fname) = 0;
    virtual bool can_still_add(uint64_t bytes, int file_count) const = 0;
};
