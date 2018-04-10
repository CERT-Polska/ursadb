#pragma once

#include <vector>
#include <iostream>
#include <fstream>

#include "Core.h"
#include "OnDiskIndex.h"
#include "Json.h"

using json = nlohmann::json;


class OnDiskDataset {
    std::vector<std::string> fnames;
    std::vector<OnDiskIndex> indices;

    const std::string &get_file_name(FileId fid);

public:
    OnDiskDataset(const std::string &fname);
    std::vector<std::string> query_primitive(const TriGram &trigram);
};
