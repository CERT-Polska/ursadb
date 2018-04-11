#pragma once

#include <vector>
#include <iostream>
#include <fstream>

#include "Core.h"
#include "OnDiskIndex.h"
#include "Json.h"
#include "Query.h"

using json = nlohmann::json;


class OnDiskDataset {
    std::string name;
    std::vector<std::string> fnames;
    std::vector<OnDiskIndex> indices;

    const std::string &get_file_name(FileId fid) const;
    void query_primitive(TriGram trigram, std::vector<FileId> &out) const;
    std::vector<FileId> query_primitive(TriGram trigram) const;
    std::vector<FileId> internal_execute(const Query &query) const;

public:
    explicit OnDiskDataset(const std::string &fname);
    const std::string &get_name() const;
    void execute(const Query &query, std::vector<std::string> &out) const;
};
