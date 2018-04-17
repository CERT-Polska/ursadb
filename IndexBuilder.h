#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "Core.h"
#include "Utils.h"


class IndexBuilder {
    std::vector<std::vector<FileId>> raw_index;
    IndexType ntype;

    void add_trigram(FileId fid, TriGram val);

public:
    IndexBuilder(IndexType ntype);

    const IndexType &index_type() { return ntype; }
    void add_file(FileId fid, const uint8_t *data, const size_t &size);
    void save(const std::string &fname);
};
