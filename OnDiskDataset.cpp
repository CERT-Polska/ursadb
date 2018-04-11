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
    std::vector<FileId> out;
    for (const FileId &fid : indices[0].query_primitive(trigram)) {
        std::cout << "pushing " << fid << " " << get_file_name(fid) << std::endl;
        out.push_back(fid);
    }
    return out;
}

QueryResult OnDiskDataset::internal_execute(const Query &query) const {
    if (query.get_type() == QueryType::PRIMITIVE) {
        return query_primitive(query.as_trigram());
    } else if (query.get_type() == QueryType::OR) {
        QueryResult result = QueryResult::empty();
        for (auto &q : query.as_queries()) {
            result.do_or(internal_execute(q));
        }
        return result;
    } else if (query.get_type() == QueryType::AND) {
        QueryResult result = QueryResult::everything();
        for (auto &q : query.as_queries()) {
            result.do_and(internal_execute(q));
        }
        return result;
    } else {
        throw std::runtime_error("Unknown query type");
    }
}

void OnDiskDataset::execute(const Query &query, std::vector<std::string> *out) const {
    QueryResult result = internal_execute(query);
    if (result.is_everything()) {
        std::copy(fnames.begin(), fnames.end(), std::back_inserter(*out));
    } else {
        for (const auto &fid : result.vector()) {
            out->push_back(get_file_name(fid));
        }
    }
}

const std::string &OnDiskDataset::get_name() const {
    return name;
}
