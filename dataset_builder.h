#pragma once

#include <vector>
#include <array>
#include <fstream>

#include "core.h"


class DatasetBuilder {
    std::vector <std::string> fids;
    std::array<std::vector<FileId>, 16777216> index;

public:
    FileId register_fname(const std::string &fname);
    void add_trigram(const FileId &fid, const TriGram &val);
    std::vector<uint8_t> compress_run(const std::vector <FileId> &files);
    void save(const std::string &fname);
};
