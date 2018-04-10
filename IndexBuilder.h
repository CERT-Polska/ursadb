#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "Core.h"


class IndexBuilder {
    std::vector<std::vector<FileId>> raw_index;

    void compress_run(const std::vector<FileId> &run, std::ofstream &out);

public:
    IndexBuilder();
    void add_trigram(const FileId &fid, const TriGram &val);
    void save(const std::string &fname);
};
