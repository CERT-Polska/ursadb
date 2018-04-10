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
    ~DatasetBuilder();

    void index(const std::string &filepath);
    void save(const std::string &fname);

private:
    std::vector<std::string> fids;
    std::vector<IndexBuilder*> indices;

    FileId register_fname(const std::string &fname);
};
