#include "OnDiskDataset.h"

#include <array>
#include <fstream>
#include <set>

#include "DatabaseName.h"
#include "Json.h"
#include "Query.h"
#include "spdlog/spdlog.h"

void OnDiskDataset::save() {
    std::set<std::string> index_names;
    for (const auto &name : indices) {
        index_names.insert(name.get_fname());
    }

    store_dataset(db_base, name, index_names, files_index->get_files_fname(),
                  files_index->get_cache_fname(), taints);
    spdlog::info("SAVE: {}", name);
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
    bool needs_save = false;
    std::string cache_fname;
    if (j.count("filename_cache") > 0) {
        cache_fname = j["filename_cache"];
    } else {
        cache_fname = "namecache." + files_fname;
        needs_save = true;
    }

    files_index.emplace(db_base, files_fname, cache_fname);

    if (needs_save) {
        save();
    }
}

std::string OnDiskDataset::get_file_name(FileId fid) const {
    return files_index->get_file_name(fid);
}

QueryResult OnDiskDataset::query(const QueryGraphCollection &graphs) const {
    QueryResult result = QueryResult::everything();
    for (auto &ndx : indices) {
        result.do_and(ndx.query(graphs.get(ndx.index_type())));
    }
    return result;
}

QueryStatistics OnDiskDataset::execute(const QueryGraphCollection &graphs,
                                       ResultWriter *out) const {
    QueryResult result = query(graphs);
    if (result.is_everything()) {
        files_index->for_each_filename(
            [&out](const std::string &fname) { out->push_back(fname); });
    } else {
        for (const auto &fid : result.vector()) {
            out->push_back(get_file_name(fid));
        }
    }
    return result.stats();
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
                          TaskSpec *task) {
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
        task->estimate_work(NUM_TRIGRAMS * index_types.size());
    }

    spdlog::debug("Pre-checks succeeded, merge can begin.");

    std::set<std::string> index_names;
    for (const auto &ndxtype : index_types) {
        spdlog::debug("Load run offsets: {}.", get_index_type_name(ndxtype));
        std::string index_name = get_index_type_name(ndxtype) + "." + outname;
        index_names.insert(index_name);
        std::vector<IndexMergeHelper> indexes;
        for (const OnDiskDataset *dataset : datasets) {
            const OnDiskIndex &index = dataset->get_index_with_type(ndxtype);
            indexes.emplace_back(
                IndexMergeHelper(&index, dataset->files_index->get_file_count(),
                                 index.read_run_offsets()));
        }
        spdlog::debug("On disk merge: {}.", get_index_type_name(ndxtype));
        OnDiskIndex::on_disk_merge(db_base, index_name, ndxtype, indexes, task);
    }

    spdlog::debug("Merge filename lists.");

    std::string fname_list = "files." + outname;
    std::ofstream of;
    of.exceptions(std::ofstream::badbit);
    of.open(db_base / fname_list, std::ofstream::binary);

    for (const OnDiskDataset *ds : datasets) {
        ds->files_index->for_each_filename(
            [&of](const std::string &fname) { of << fname << "\n"; });
    }
    of.flush();

    store_dataset(db_base, outname, index_names, fname_list, std::nullopt,
                  datasets[0]->get_taints());

    spdlog::debug("Merge finished successfully.");
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
    drop_file(files_index->get_cache_fname());
    drop_file(get_name());
}

fs::path OnDiskDataset::get_base() const { return db_base; }

std::vector<std::vector<const OnDiskDataset *>>
OnDiskDataset::get_compatible_datasets(
    const std::vector<const OnDiskDataset *> &datasets) {
    std::map<std::set<std::string>, std::vector<const OnDiskDataset *>> partial;
    for (auto ds : datasets) {
        std::set<std::string> ds_class;
        for (const auto &taint : ds->get_taints()) {
            ds_class.insert("taint:" + taint);
        }
        for (const auto &index : ds->get_indexes()) {
            ds_class.insert("type:" + get_index_type_name(index.index_type()));
        }
        partial[ds_class].push_back(ds);
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

QueryGraphCollection::QueryGraphCollection(
    const Query &query, const std::unordered_set<IndexType> &types) {
    graphs_.reserve(types.size());
    for (const auto type : types) {
        graphs_.emplace(type, std::move(query.to_graph(type)));
    }
}

const QueryGraph &QueryGraphCollection::get(IndexType type) const {
    const auto it = graphs_.find(type);
    if (it == graphs_.end()) {
        throw std::runtime_error(
            "QueryGraphCollection doesn't contain a graph of the requested "
            "type");
    }
    return it->second;
}
