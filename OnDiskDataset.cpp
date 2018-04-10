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
    query_primitive(trigram, res);
    return res;
}

void OnDiskDataset::query_primitive(const TriGram &trigram, std::vector<std::string> &out) {
    std::cout << "query_primitive(" << trigram << ")" << std::endl;

    for (FileId fid : indices[0].query_primitive(trigram)) {
        std::cout << "pushing " << get_file_name(fid) << std::endl;
        out.push_back(get_file_name(fid));
    }
}
