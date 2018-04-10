#include "OnDiskDataset.h"

OnDiskDataset::OnDiskDataset(const std::string &fname) {
    std::ifstream in(fname);
    json j;
    in >> j;

    for (std::string index_fname : j["indices"]) {
        indices.emplace_back(index_fname);
    }

    for (std::string filename : j["filenames"]) {
        fnames.push_back(filename);
    }
}

const std::string &OnDiskDataset::get_file_name(FileId fid) {
    return fnames.at(fid);
}

std::vector<std::string> OnDiskDataset::query_primitive(const TriGram &trigram) {
    std::vector<std::string> res;

    for (FileId fid : indices[0].query_primitive(trigram)) {
        res.push_back(get_file_name(fid));
    }

    return res;
}
