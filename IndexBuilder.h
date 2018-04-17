#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "Core.h"
#include "Utils.h"

class IndexBuilder {
    std::vector<std::vector<FileId>> raw_index;
    IndexType ntype;

    void add_trigram(FileId fid, TriGram val);

  public:
    IndexBuilder(IndexType ntype);

    IndexType index_type() { return ntype; }
    void add_file(FileId fid, const uint8_t *data, size_t size);
    void save(const std::string &fname);
};
