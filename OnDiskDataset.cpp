#include "OnDiskDataset.h"
#include "Query.h"

OnDiskDataset::OnDiskDataset(const std::string &fname) :name(fname) {
    std::ifstream in(name);
    json j;
    in >> j;

    for (std::string index_fname : j["indices"]) {
        indices.emplace_back(index_fname);
    }

    for (std::string filename : j["filenames"]) {
        fnames.push_back(filename);
    }
}

const std::string &OnDiskDataset::get_file_name(FileId fid) const {
    return fnames.at(fid);
}

std::vector<FileId> OnDiskDataset::query_primitive(TriGram trigram) const {
    std::vector<FileId> res;
    query_primitive(trigram, res);
    return res;
}

void OnDiskDataset::query_primitive(TriGram trigram, std::vector<FileId> &out) const {
    std::cout << "query_primitive(" << trigram << ")" << std::endl;

    for (const FileId &fid : indices[0].query_primitive(trigram)) {
        std::cout << "pushing " << fid << " " << get_file_name(fid) << std::endl;
        out.push_back(fid);
    }
}

std::vector<FileId> OnDiskDataset::internal_execute(const Query &query) const {
    std::vector<FileId> out;

    if (query.get_type() == QueryType::PRIMITIVE) {
        query_primitive(query.as_trigram(), out);
    } else if (query.get_type() == QueryType::OR) {
        for (auto &q : query.as_queries()) {
            std::vector<FileId> new_out;
            std::vector<FileId> partial = internal_execute(q);
            std::set_union(partial.begin(), partial.end(), out.begin(), out.end(), std::back_inserter(new_out));
            out = new_out;
        }
    } else if (query.get_type() == QueryType::AND) {
        bool is_first = true;

        for (auto &q : query.as_queries()) {
            std::vector<FileId> new_out;
            std::vector<FileId> partial = internal_execute(q);

            if (!is_first) {
                std::set_intersection(partial.begin(), partial.end(), out.begin(), out.end(),
                                      std::back_inserter(new_out));
                out = new_out;
            } else {
                is_first = false;
                out = partial;
            }
        }
    }

    return out;
}

void OnDiskDataset::execute(const Query &query, std::vector<std::string> &out) const {
    std::vector<FileId> fids = internal_execute(query);

    for (const auto &fid : fids) {
        out.push_back(get_file_name(fid));
    }
}

const std::string &OnDiskDataset::get_name() const {
    return name;
}
