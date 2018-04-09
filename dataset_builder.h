#pragma once

#include <vector>
#include <array>
#include <fstream>

#include "core.h"


class DatasetBuilder {
public:
    DatasetBuilder() :run_offsets(NUM_TRIGRAMS) {
    }

    void index(const std::string &filepath);
    void save(const std::string &fname);
private:
    std::vector<std::string> fids;
    std::vector<std::vector<FileId>> run_offsets;

    FileId register_fname(const std::string &fname);
    std::vector<uint8_t> compress_run(const std::vector<FileId> &files);
    void add_trigram(const FileId &fid, const TriGram &val);
};
