#pragma once

#include <string>
#include <vector>

#include "Core.h"

class IndexBuilder {
    std::vector<uint8_t> raw_data;
    IndexType ntype;

    void add_trigram(FileId fid, TriGram val);
    std::vector<FileId> get_run(TriGram val) const;

  public:
    IndexBuilder(IndexType ntype);

    IndexType index_type() const { return ntype; }
    void add_file(FileId fid, const uint8_t *data, size_t size);
    void save(const std::string &fname) const;
    bool must_spill(int file_count) const;
};
