#include "DatasetBuilder.h"

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <set>

#include "OnDiskDataset.h"
#include "Utils.h"
#include "Json.h"
#include "MemMap.h"
#include "BitmapIndexBuilder.h"

DatasetBuilder::DatasetBuilder(BuilderType builder_type, const std::vector<IndexType> &index_types)
        : builder_type(builder_type), index_types(index_types) {
    clear();
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
        std::string ndx_name = get_index_type_name(ndx->index_type()) + "." + fname;
        ndx->save(db_base / ndx_name);
        index_names.emplace(ndx_name);
    }

    std::string fname_list = "files." + fname;

    std::ofstream of;
    of.exceptions(std::ofstream::badbit);
    of.open(db_base / fname_list, std::ofstream::binary);

    for (const std::string &filename : fids) {
        of << filename << "\n";
    }
    of.flush();

    store_dataset(db_base, fname, index_names, fname_list);
}

void DatasetBuilder::force_registered(const std::string &filepath) {
    if (!fids.empty()) {
        if (fids.back() == filepath) {
            return;
        }
    }

    register_fname(filepath);
}

void DatasetBuilder::index(const std::string &filepath) {
    if (filepath.find('\r') != std::string::npos || filepath.find('\n') != std::string::npos) {
        throw invalid_filename_error(filepath);
    }

    MemMap in(filepath);

    FileId fid = register_fname(filepath);

    for (auto &ndx : indices) {
        ndx->add_file(fid, in.data(), in.size());
    }
}

bool DatasetBuilder::can_still_add(uint64_t bytes) const {
    for (const auto &ndx : indices) {
        if (!ndx->can_still_add(bytes, fids.size())) {
            return false;
        }
    }
    return true;
}

void DatasetBuilder::clear() {
    indices.clear();
    fids.clear();

    for (const auto &index_type : index_types) {
        if (builder_type == BuilderType::FLAT) {
            indices.emplace_back(new FlatIndexBuilder(index_type));
        } else if (builder_type == BuilderType::BITMAP) {
            indices.emplace_back(new BitmapIndexBuilder(index_type));
        } else {
            throw std::runtime_error("unhandled builder type");
        }
    }
}

const char *invalid_filename_error::what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_USE_NOEXCEPT {
    return what_message.c_str();
}
