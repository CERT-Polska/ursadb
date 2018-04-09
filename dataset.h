#pragma once

#include <vector>
#include <iostream>
#include <fstream>

#include "core.h"


class OnDiskDataset {
public:
    OnDiskDataset() :run_offsets(NUM_TRIGRAMS) {
    }

    void load(const std::string &fname);
    const std::string &get_file_name(FileId fid);
    std::vector<FileId> read_compressed_run(std::ifstream &runs, size_t len);
    std::vector<FileId> query_index(const TriGram &trigram);

private:
    std::vector<std::string> fnames;
    std::ifstream raw_data;
    std::vector<uint32_t> run_offsets;
};
