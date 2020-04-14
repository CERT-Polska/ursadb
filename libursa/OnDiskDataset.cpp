#include "OnDiskDataset.h"

#include <array>
#include <fstream>
#include <set>

#include "Database.h"
#include "Json.h"
#include "Query.h"

void OnDiskDataset::save() {
    std::set<std::string> index_names;
    for (const auto &name : indices) {
        index_names.insert(name.get_fname());
    }

    store_dataset(db_base, name, index_names, files_index->get_files_fname(),
                  taints);
}

void OnDiskDataset::toggle_taint(const std::string &taint) {
    if (taints.count(taint) > 0) {
        taints.erase(taint);
    } else {
        taints.insert(taint);
    }
}

OnDiskDataset::OnDiskDataset(const fs::path &db_base, const std::string &fname)
    : name(fname), db_base(db_base) {
    std::ifstream in(db_base / name, std::ifstream::binary);
    json j;
    in >> j;

    for (const std::string &index_fname : j["indices"]) {
        indices.emplace_back(db_base / index_fname);
    }

    for (const std::string &taint : j["taints"]) {
        taints.insert(taint);
    }

    std::string files_fname = j["files"];
    files_index.emplace(db_base, files_fname);
}

std::string OnDiskDataset::get_file_name(FileId fid) const {
    return files_index->get_file_name(fid);
}

QueryResult OnDiskDataset::query_str(const QString &str) const {
    QueryResult result = QueryResult::everything();

    for (auto &ndx : indices) {
        result.do_and(ndx.query_str(str));
    }

    return result;
}

std::vector<FileId> internal_pick_common(
    int cutoff, const std::vector<const std::vector<FileId> *> &sources) {
    // returns all FileIds which appear at least `cutoff` times among provided
    // `sources`
    using FileIdRange = std::pair<std::vector<FileId>::const_iterator,
                                  std::vector<FileId>::const_iterator>;
    std::vector<FileId> result;
    std::vector<FileIdRange> heads;
    heads.reserve(sources.size());

    for (auto source : sources) {
        if (!source->empty()) {
            heads.emplace_back(
                std::make_pair(source->cbegin(), source->cend()));
        }
    }

    while (static_cast<int>(heads.size()) >= cutoff) {
        // pick lowest possible FileId value among all current heads
        int min_index = 0;
        FileId min_id = *heads[0].first;
        for (int i = 1; i < static_cast<int>(heads.size()); i++) {
            if (*heads[i].first < min_id) {
                min_index = i;  // TODO benchmark and consider removing.
                min_id = *heads[i].first;
            }
        }

        // fix on that particular value selected in previous step and count
        // number of repetitions among heads.
        // Note that it's implementation-defined that std::vector<FileId>
        // is always sorted and we use this fact here.
        int repeat_count = 0;
        for (int i = min_index; i < static_cast<int>(heads.size()); i++) {
            if (*heads[i].first == min_id) {
                repeat_count += 1;
                heads[i].first++;
                // head ended, we may get rid of it
                if (heads[i].first == heads[i].second) {
                    heads.erase(heads.begin() + i);
                    i--;  // Be careful not to skip elements!
                }
            }
        }

        // this value has enough repetitions among different heads to add it to
        // the result set
        if (repeat_count >= cutoff) {
            result.push_back(min_id);
        }
    }

    return result;
}

QueryResult OnDiskDataset::pick_common(
    int cutoff, const std::vector<Query> &queries) const {
    if (cutoff > static_cast<int>(queries.size())) {
        // Short circuit when cutoff is too big.
        // This should never happen for well-formed queries, but this check is
        // very cheap.
        return QueryResult::empty();
    }
    if (cutoff <= 0) {
        // '0 of (...)' is considered as matching-everything.
        return QueryResult::everything();
    }

    std::vector<QueryResult> sources_storage;
    for (auto &query : queries) {
        QueryResult result = internal_execute(query);
        if (result.is_everything()) {
            cutoff -= 1;
            if (cutoff <= 0) {
                // Short circuit when result is trivially everything().
                // Do it in the loop, to potentially save expensive
                // `internal_execute` invocations.
                return QueryResult::everything();
            }
        } else {
            sources_storage.push_back(std::move(result));
        }
    }

    // Special case optimization for cutoff==1 and a single source.
    if (cutoff == 1 && sources_storage.size() == 1) {
        return std::move(sources_storage[0]);
    }

    std::vector<const std::vector<FileId> *> sources;
    for (auto &s : sources_storage) {
        sources.push_back(&s.vector());
    }

    return QueryResult(internal_pick_common(cutoff, sources));
}

QueryResult OnDiskDataset::internal_execute(const Query &query) const {
    switch (query.get_type()) {
        case QueryType::PRIMITIVE:
            return query_str(query.as_value());
        case QueryType::OR: {
            return pick_common(1, query.as_queries());
        }
        case QueryType::AND: {
            return pick_common(query.as_queries().size(), query.as_queries());
        }
        case QueryType::MIN_OF: {
            return pick_common(query.as_count(), query.as_queries());
        }
    }

    throw std::runtime_error("unhandled query type");
}

void OnDiskDataset::execute(const Query &query, ResultWriter *out) const {
    QueryResult result = internal_execute(query);
    if (result.is_everything()) {
        files_index->for_each_filename(
            [&out](const std::string &fname) { out->push_back(fname); });
    } else {
        for (const auto &fid : result.vector()) {
            out->push_back(get_file_name(fid));
        }
    }
}

bool OnDiskDataset::has_all_taints(const std::set<std::string> &taints) const {
    for (const auto &taint : taints) {
        if (this->taints.count(taint) == 0) {
            return false;
        }
    }
    return true;
}

const std::string &OnDiskDataset::get_name() const { return name; }

std::string OnDiskDataset::get_id() const {
    // TODO use DatabaseName as a class member instead
    auto dbname = DatabaseName::parse(db_base, name);
    return dbname.get_id();
}

void OnDiskDataset::merge(const fs::path &db_base, const std::string &outname,
                          const std::vector<const OnDiskDataset *> &datasets,
                          Task *task) {
    std::set<IndexType> index_types;

    if (datasets.size() < 2) {
        throw std::runtime_error("merge requires at least 2 datasets");
    }

    for (size_t i = 1; i < datasets.size(); i++) {
        if (!datasets[0]->is_taint_compatible(*datasets[i])) {
            std::stringstream ss;
            ss << "trying to merge \"" << datasets[0]->get_name() << "\" and \""
               << datasets[i]->get_name() << "\" ";
            ss << "but they have different taints - aborting";
            throw std::runtime_error(ss.str());
        }
    }

    for (const OnDiskIndex &index : datasets[0]->indices) {
        index_types.insert(index.index_type());
    }

    for (size_t i = 1; i < datasets.size(); i++) {
        std::set<IndexType> tmp_types;

        for (const OnDiskIndex &index : datasets[i]->indices) {
            tmp_types.insert(index.index_type());
        }

        if (tmp_types != index_types) {
            std::stringstream ss;
            ss << "trying to merge \"" << datasets[0]->get_name() << "\" and \""
               << datasets[i]->get_name() << "\" ";
            ss << "but these ones contain index(es) of different type(s)";
            throw std::runtime_error(ss.str());
        }
    }

    if (task != nullptr) {
        task->work_estimated = NUM_TRIGRAMS * index_types.size();
    }

    std::set<std::string> index_names;
    for (IndexType index_type : index_types) {
        std::string index_name =
            get_index_type_name(index_type) + "." + outname;
        index_names.insert(index_name);
        std::vector<IndexMergeHelper> indexes;
        for (const OnDiskDataset *dataset : datasets) {
            const OnDiskIndex &index = dataset->get_index_with_type(index_type);
            indexes.emplace_back(
                IndexMergeHelper(&index, dataset->files_index->get_file_count(),
                                 index.read_run_offsets()));
        }
        OnDiskIndex::on_disk_merge(db_base, index_name, index_type, indexes,
                                   task);
    }

    std::string fname_list = "files." + outname;
    std::ofstream of;
    of.exceptions(std::ofstream::badbit);
    of.open(db_base / fname_list, std::ofstream::binary);

    for (const OnDiskDataset *ds : datasets) {
        ds->files_index->for_each_filename(
            [&of](const std::string &fname) { of << fname << "\n"; });
    }
    of.flush();

    store_dataset(db_base, outname, index_names, fname_list,
                  datasets[0]->get_taints());
}

const OnDiskIndex &OnDiskDataset::get_index_with_type(
    IndexType index_type) const {
    for (const OnDiskIndex &index : indices) {
        if (index.index_type() == index_type) {
            return index;
        }
    }
    throw std::runtime_error("Requested index type doesn't exist in dataset");
}

void OnDiskDataset::drop_file(const std::string &fname) const {
    // it may happen that dataset was reloaded and then is scheduled for removal
    // multiple times so we have to account for that and only delete yet
    // existing files
    fs::remove(db_base / fname);
}

void OnDiskDataset::drop() {
    std::vector<std::string> idx_names;

    for (auto it = indices.begin(); it != indices.end(); ++it) {
        idx_names.push_back((*it).get_fname());
    }

    // deallocate objects to close FDs
    indices.clear();

    for (auto &idx_name : idx_names) {
        drop_file(idx_name);
    }

    drop_file(files_index->get_files_fname());
    drop_file(get_name());
}

fs::path OnDiskDataset::get_base() const { return db_base; }

std::vector<std::vector<const OnDiskDataset *>>
OnDiskDataset::get_taint_compatible_datasets(
    const std::vector<const OnDiskDataset *> &datasets) {
    std::map<std::set<std::string>, std::vector<const OnDiskDataset *>> partial;
    for (auto ds : datasets) {
        partial[ds->get_taints()].push_back(ds);
    }

    std::vector<std::vector<const OnDiskDataset *>> result;
    for (const auto &kv : partial) {
        result.push_back(kv.second);
    }

    return result;
}

std::vector<const OnDiskDataset *> OnDiskDataset::get_compact_candidates(
    const std::vector<const OnDiskDataset *> &datasets) {
    std::vector<const OnDiskDataset *> out;

    struct DatasetScore {
        const OnDiskDataset *ds;
        uint64_t size;

        DatasetScore(const OnDiskDataset *ds, unsigned long size)
            : ds(ds), size(size) {}
    };

    struct compare_size {
        bool operator()(const DatasetScore &lhs,
                        const DatasetScore &rhs) const {
            return lhs.size < rhs.size;
        }
    };

    if (datasets.size() < 2) {
        return out;
    }

    std::vector<DatasetScore> scores;

    for (auto *ds : datasets) {
        uint64_t dataset_size = 0;

        for (const auto &ndx : ds->get_indexes()) {
            dataset_size += fs::file_size(ds->get_base() / ndx.get_fname());
        }

        scores.emplace_back(ds, dataset_size);
    }

    std::sort(scores.begin(), scores.end(), compare_size());

    out.push_back(scores[0].ds);
    unsigned int offset = 1;

    while (offset < scores.size() &&
           scores[offset - 1].size * 2 > scores[offset].size) {
        out.push_back(scores[offset].ds);
        offset++;
    }

    if (out.size() < 2) {
        // no candidate to merge with smallest dataset
        out.clear();
    }

    return out;
}
