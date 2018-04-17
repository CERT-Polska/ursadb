#include <fstream>
#include <iostream>

#include "DatasetBuilder.h"

#include "Utils.h"
#include "OnDiskDataset.h"


DatasetBuilder::DatasetBuilder() : total_bytes(0) {
    indices.emplace_back(IndexType::GRAM3);
}

FileId DatasetBuilder::register_fname(const std::string &fname) {
    auto new_id = (FileId) fids.size();
    fids.push_back(fname);
    return new_id;
}

void DatasetBuilder::save(const std::string &fname) {
    std::set<std::string> c_set;

    for (auto &ndx : indices) {
        std::string ndx_name = get_index_type_name(ndx.index_type()) + "." + fname;
        ndx.save(ndx_name);
        c_set.emplace(ndx_name);
    }

    json dataset;

    json j_indices(c_set);
    json j_fids(fids);

    dataset["indices"] = j_indices;
    dataset["filenames"] = j_fids;

    std::ofstream o(fname, std::ofstream::out);
    o << std::setw(4) << dataset << std::endl;
    o.close();
}

void DatasetBuilder::index(const std::string &filepath) {
    MemMap in(filepath);

    if (in.size() > 1024*1024*128) {
        std::cout << "!!! TEMPORARY HACK, FILE TOO LARGE: " << filepath << std::endl;
        return;
    }

    total_bytes += in.size();
    FileId fid = register_fname(filepath);

    for (auto &ndx : indices) {
        ndx.add_file(fid, in.data(), in.size());
    }
}
