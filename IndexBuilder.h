#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "Core.h"


struct LinkedFile {
    FileId fid;
    LinkedFile *next;

    explicit LinkedFile(FileId fid) : fid(fid), next(nullptr) {}
};


class IndexBuilder {
    LinkedFile *raw_index[NUM_TRIGRAMS];

    void compress_run(LinkedFile *run, std::ofstream &out);

public:
    ~IndexBuilder();
    void add_trigram(const FileId &fid, const TriGram &val);
    void save(const std::string &fname);
};
