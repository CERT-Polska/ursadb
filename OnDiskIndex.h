#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "Core.h"

enum IndexType {
    GRAM3 = 1
};

class OnDiskIndex {
    uint32_t run_offsets[NUM_TRIGRAMS];
    std::ifstream raw_data;
    IndexType ntype;

    std::vector<FileId> read_compressed_run(std::ifstream &runs, size_t len);

public:
    OnDiskIndex(const std::string &fname);
    std::vector<FileId> query_primitive(const TriGram &trigram);
};
