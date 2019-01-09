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
    virtual bool must_spill(int file_count) const = 0;
};
