#pragma once

#include <string>
#include <vector>

#include "Core.h"
#include "IndexBuilder.h"

class BitmapIndexBuilder : public IndexBuilder {
    std::vector<uint8_t> raw_data;

    void add_trigram(FileId fid, TriGram val);
    std::vector<FileId> get_run(TriGram val) const;

  public:
    BitmapIndexBuilder(IndexType ntype);

    void add_file(FileId fid, const uint8_t *data, size_t size);
    void save(const std::string &fname);
    bool must_spill(int file_count) const;
};
