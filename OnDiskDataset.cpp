#include "OnDiskDataset.h"

#include <set>

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

void OnDiskDataset::merge(const std::string &outname, const std::vector<OnDiskDataset> &datasets) {
    std::set<IndexType> index_types;

    for (const OnDiskDataset &dataset : datasets) {
        for (const OnDiskIndex &index : dataset.indices) {
            index_types.insert(index.index_type());
        }
    }

    json dataset;

    std::vector<std::string> index_names;
    for (IndexType index_type : index_types) {
        std::string index_name = outname + "." + get_index_type_name(index_type) + ".ursa";
        index_names.push_back(index_name);
        std::vector<IndexMergeHelper> indexes;
        for (const OnDiskDataset &dataset : datasets) {
            indexes.push_back(IndexMergeHelper(
                &dataset.get_index_with_type(index_type),
                dataset.fnames.size()
            ));
        }
        OnDiskIndex::on_disk_merge(index_name, index_type, indexes);
    }

    std::vector<std::string> file_names;
    for (const OnDiskDataset &dataset : datasets) {
        for (const std::string fname : dataset.fnames) {
            file_names.push_back(fname);
        }
    }

    json j_indices(index_names);
    json j_fids(file_names);

    dataset["indices"] = j_indices;
    dataset["filenames"] = j_fids;

    std::ofstream o(outname);
    o << std::setw(4) << dataset << std::endl;
    o.close();
}

const OnDiskIndex &OnDiskDataset::get_index_with_type(IndexType index_type) const {
    for (const OnDiskIndex &index : indices) {
        if (index.index_type() == index_type) {
            return index;
        }
    }
    throw std::runtime_error("Requested index type doesn't exist in dataset");
}