#pragma once

#include <vector>
#include <set>
#include <array>
#include <fstream>

#include "Core.h"
#include "IndexBuilder.h"


class DatasetBuilder {
public:
    DatasetBuilder();

    void index(const std::string &filepath);
    void save(const std::string &fname);
    const long &processed_bytes() { return total_bytes; }

private:
    std::vector<std::string> fids;
    std::vector<IndexBuilder> indices;
    long total_bytes;

    FileId register_fname(const std::string &fname);

    void index_path(const std::string &filepath);
};
