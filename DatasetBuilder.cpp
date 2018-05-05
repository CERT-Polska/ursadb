#include "DatasetBuilder.h"

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <set>

#include "OnDiskDataset.h"
#include "Utils.h"
#include "Json.h"

DatasetBuilder::DatasetBuilder(const std::vector<IndexType> &index_types) {
    for (const auto &index_type : index_types) {
        indices.emplace_back(index_type);
    }
}

FileId DatasetBuilder::register_fname(const std::string &fname) {
    if (fname.find('\n') != std::string::npos || fname.find('\r') != std::string::npos) {
        throw std::runtime_error("file name contains invalid character (either \\r or \\n)");
    }

    auto new_id = (FileId)fids.size();
    fids.push_back(fname);
    return new_id;
}

void DatasetBuilder::save(const fs::path &db_base, const std::string &fname) {
    std::set<std::string> index_names;

    for (auto &ndx : indices) {
        std::string ndx_name = get_index_type_name(ndx.index_type()) + "." + fname;
        ndx.save(db_base / ndx_name);
        index_names.emplace(ndx_name);
    }

    store_dataset(db_base, fname, index_names, fids);
}

void DatasetBuilder::index(const std::string &filepath) {
    MemMap in(filepath);

    FileId fid = register_fname(filepath);

    for (auto &ndx : indices) {
        ndx.add_file(fid, in.data(), in.size());
    }
}

bool DatasetBuilder::must_spill() {
    for (const auto &ndx : indices) {
        if (ndx.must_spill(fids.size())) {
            return true;
        }
    }
    return false;
}
