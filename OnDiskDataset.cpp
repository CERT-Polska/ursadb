#include "OnDiskDataset.h"

#include <fstream>
#include <set>

#include "Database.h"
#include "Query.h"
#include "Json.h"

OnDiskDataset::OnDiskDataset(const fs::path &db_base, const std::string &fname)
    : name(fname), db_base(db_base) {
    std::ifstream in(db_base / name, std::ifstream::binary);
    json j;
    in >> j;

    for (std::string index_fname : j["indices"]) {
        indices.emplace_back(db_base / index_fname);
    }

    files_fname = j["files"];
    std::string filename;
    std::ifstream inf(db_base / files_fname, std::ifstream::binary);

    while (!inf.eof()) {
        std::getline(inf, filename);

        if (!filename.empty()) {
            fnames.push_back(filename);
        }
    }
}

const std::string &OnDiskDataset::get_file_name(FileId fid) const { return fnames.at(fid); }

QueryResult OnDiskDataset::query_str(const std::string &str) const {
    QueryResult result = QueryResult::everything();

    for (auto &ndx : indices) {
        result.do_and(ndx.query_str(str));
    }

    return result;
}

QueryResult OnDiskDataset::internal_execute(const Query &query) const {
    switch (query.get_type()) {
        case QueryType::PRIMITIVE:
            return query_str(query.as_value());
        case QueryType::OR: {
            QueryResult result = QueryResult::empty();
            for (auto &q : query.as_queries()) {
                result.do_or(internal_execute(q));
            }
            return result;
        }
        case QueryType::AND: {
            QueryResult result = QueryResult::everything();
            for (auto &q : query.as_queries()) {
                result.do_and(internal_execute(q));
            }
            return result;
        }
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

const std::string &OnDiskDataset::get_name() const { return name; }

std::string OnDiskDataset::get_id() const {
    int first_dot = name.find('.');
    int second_dot = name.find('.', first_dot + 1);
    if (second_dot == std::string::npos) {
        throw std::runtime_error("Invalid dataset ID found");
    }
    return name.substr(first_dot + 1, second_dot - first_dot - 1);
}

void OnDiskDataset::merge(
        const fs::path &db_base, const std::string &outname,
        const std::vector<const OnDiskDataset *> &datasets, Task *task) {
    std::set<IndexType> index_types;

    if (datasets.size() < 2) {
        throw std::runtime_error("merge requires at least 2 datasets");
    }

    for (const OnDiskIndex &index : datasets[0]->indices) {
        index_types.insert(index.index_type());
    }

    for (int i = 1; i < datasets.size(); i++) {
        std::set<IndexType> tmp_types;

        for (const OnDiskIndex &index : datasets[i]->indices) {
            tmp_types.insert(index.index_type());
        }

        if (tmp_types != index_types) {
            std::stringstream ss;
            ss << "trying to merge \"" << datasets[0]->get_name() << "\" and \"" << datasets[i]->get_name() << "\" ";
            ss << "but these ones contain index(es) of different type(s)";
            throw std::runtime_error(ss.str());
        }
    }

    task->work_estimated = NUM_TRIGRAMS * index_types.size();

    json dataset;

    std::set<std::string> index_names;
    for (IndexType index_type : index_types) {
        std::string index_name = get_index_type_name(index_type) + "." + outname;
        index_names.insert(index_name);
        std::vector<IndexMergeHelper> indexes;
        for (const OnDiskDataset *dataset : datasets) {
            indexes.push_back(IndexMergeHelper(
                    &dataset->get_index_with_type(index_type), dataset->fnames.size()));
        }
        OnDiskIndex::on_disk_merge(db_base, index_name, index_type, indexes, task);
    }

    std::vector<std::string> file_names;
    for (const OnDiskDataset *dataset : datasets) {
        for (const std::string fname : dataset->fnames) {
            file_names.push_back(fname);
        }
    }

    store_dataset(db_base, outname, index_names, file_names);
}

const OnDiskIndex &OnDiskDataset::get_index_with_type(IndexType index_type) const {
    for (const OnDiskIndex &index : indices) {
        if (index.index_type() == index_type) {
            return index;
        }
    }
    throw std::runtime_error("Requested index type doesn't exist in dataset");
}

void OnDiskDataset::drop_file(const std::string &fname) const {
    // it may happen that dataset was reloaded and then is scheduled for removal multiple times
    // so we have to account for that and only delete yet existing files

    if (fs::exists(fname)) {
        fs::remove(fname);
    }
}

void OnDiskDataset::drop() {
    std::vector<std::string> idx_names;

    for (auto it = indices.begin(); it != indices.end(); ++it) {
        idx_names.push_back((*it).get_fname());
    }

    // deallocate objects to close MemMaps
    indices.clear();

    for (auto &idx_name : idx_names) {
        drop_file(idx_name);
    }

    drop_file(db_base / files_fname);
    drop_file(db_base / get_name());
}
