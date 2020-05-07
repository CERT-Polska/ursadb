#include "DatabaseSnapshot.h"

#include <fstream>
#include <stdexcept>
#include <utility>

#include "ExclusiveFile.h"
#include "Indexer.h"
#include "Utils.h"
#include "spdlog/spdlog.h"

DatabaseSnapshot::DatabaseSnapshot(
    fs::path db_name, fs::path db_base, DatabaseConfig config,
    std::map<std::string, OnDiskIterator> iterators,
    std::vector<const OnDiskDataset *> datasets,
    std::unordered_map<uint64_t, TaskSpec> tasks)
    : db_name(std::move(db_name)),
      db_base(std::move(db_base)),
      iterators(std::move(iterators)),
      config(std::move(config)),
      datasets(std::move(datasets)),
      tasks(std::move(tasks)) {}

const OnDiskDataset *DatabaseSnapshot::find_dataset(
    const std::string &name) const {
    for (const auto &ds : datasets) {
        if (ds->get_id() == name) {
            return ds;
        }
    }
    return nullptr;
}

void DatabaseSnapshot::read_iterator(Task *task, const std::string &iterator_id,
                                     int count, std::vector<std::string> *out,
                                     uint64_t *out_iterator_position,
                                     uint64_t *out_iterator_files) const {
    *out_iterator_files = 0;
    *out_iterator_position = 0;

    auto it = iterators.find(iterator_id);
    if (it == iterators.end()) {
        throw std::runtime_error("tried to read non-existent iterator");
    }

    OnDiskIterator iterator_copy = it->second;
    iterator_copy.pop(count, out);
    *out_iterator_files = iterator_copy.get_total_files();
    *out_iterator_position = iterator_copy.get_file_offset();

    std::string byte_offset = std::to_string(iterator_copy.get_byte_offset());
    std::string file_offset = std::to_string(iterator_copy.get_file_offset());
    std::string param = byte_offset + ":" + file_offset;
    task->change(DBChange(DbChangeType::UpdateIterator,
                          iterator_copy.get_name().get_filename(), param));
}

void DatabaseSnapshot::build_target_list(
    const std::string &filepath, const std::set<std::string> &existing_files,
    std::vector<std::string> *targets) const {
    fs::recursive_directory_iterator end;

    if (fs::is_regular_file(filepath)) {
        fs::path absfn = fs::absolute(filepath);

        if (existing_files.count(absfn) == 0) {
            targets->push_back(absfn);
        }
    } else {
        for (fs::recursive_directory_iterator dir(filepath); dir != end;
             ++dir) {
            if (fs::is_regular_file(dir->path())) {
                fs::path absfn = fs::absolute(dir->path());
                if (existing_files.count(absfn) == 0) {
                    targets->push_back(absfn);
                }
            }
        }
    }
}

void DatabaseSnapshot::find_all_indexed_files(
    std::set<std::string> *existing_files) const {
    for (const auto &ds : datasets) {
        ds->for_each_filename([&existing_files](const std::string &fname) {
            existing_files->insert(fname);
        });
    }
}

void DatabaseSnapshot::recursive_index_paths(
    Task *task, const std::vector<IndexType> &types,
    const std::vector<std::string> &root_paths) const {
    std::vector<std::string> targets;
    {
        std::set<std::string> existing_files;
        find_all_indexed_files(&existing_files);
        for (const auto &filepath : root_paths) {
            build_target_list(filepath, existing_files, &targets);
        }
    }
    force_index_files(task, types, targets);
}

void DatabaseSnapshot::force_recursive_index_paths(
    Task *task, const std::vector<IndexType> &types,
    const std::vector<std::string> &root_paths) const {
    std::vector<std::string> targets;
    for (const auto &filepath : root_paths) {
        build_target_list(filepath, {}, &targets);
    }
    force_index_files(task, types, targets);
}

void DatabaseSnapshot::index_files(
    Task *task, const std::vector<IndexType> &types,
    const std::vector<std::string> &filenames) const {
    std::vector<std::string> unique_filenames;
    {
        std::set<std::string> existing_files;
        find_all_indexed_files(&existing_files);
        for (const auto &filename : filenames) {
            if (existing_files.count(filename) == 0) {
                unique_filenames.push_back(filename);
            }
        }
    }
    force_index_files(task, types, unique_filenames);
}

void DatabaseSnapshot::force_index_files(
    Task *task, const std::vector<IndexType> &types,
    const std::vector<std::string> &targets) const {
    if (targets.empty()) {
        return;
    }

    Indexer indexer(this, types);

    task->spec().estimate_work(targets.size() + 1);

    for (const auto &target : targets) {
        spdlog::debug("Indexing {}", target);
        indexer.index(target);
        task->spec().add_progress(1);
    }

    for (const auto *ds : indexer.finalize()) {
        task->change(DBChange(DbChangeType::Insert, ds->get_name()));
    }

    task->spec().add_progress(1);
}

void DatabaseSnapshot::reindex_dataset(Task *task,
                                       const std::vector<IndexType> &types,
                                       const std::string &dataset_name) const {
    const OnDiskDataset *source = find_dataset(dataset_name);

    if (source == nullptr) {
        throw std::runtime_error("source dataset was not found");
    }

    Indexer indexer(this, types);

    task->spec().estimate_work(source->get_file_count() + 1);

    source->for_each_filename([&indexer, &task](const std::string &target) {
        spdlog::debug("Reindexing {}", target);
        indexer.index(target);
        task->spec().add_progress(1);
    });

    for (const auto *ds : indexer.finalize()) {
        task->change(DBChange(DbChangeType::Insert, ds->get_name()));
        for (const auto &taint : source->get_taints()) {
            task->change(
                DBChange(DbChangeType::ToggleTaint, ds->get_name(), taint));
        }
    }

    task->change(DBChange(DbChangeType::Drop, source->get_id()));
    task->spec().add_progress(1);
}

DatabaseName DatabaseSnapshot::allocate_name(const std::string &type) const {
    while (true) {
        // TODO(unknown): limit this to some sane value (like 10000 etc),
        // to avoid infinite loop in exceptional cases.

        std::stringstream ss;
        std::string id(random_hex_string(8));
        ss << type << "." << id << "." << db_name.string();
        std::string fname = ss.str();
        ExclusiveFile lock(db_base / fname);
        if (lock.is_ok()) {
            return DatabaseName(db_base, type, id, fname);
        }
    }
}

QueryCounters DatabaseSnapshot::execute(const Query &query,
                                        const std::set<std::string> &taints,
                                        const std::set<std::string> &datasets,
                                        Task *task, ResultWriter *out) const {
    std::vector<const OnDiskDataset *> datasets_to_query;
    if (datasets.empty()) {
        // No datasets selected explicitly == query everything.
        datasets_to_query = get_datasets();
    } else {
        datasets_to_query.reserve(datasets.size());
        for (const auto &dsname : datasets) {
            const auto *dsptr = find_dataset(dsname);
            if (dsptr == nullptr) {
                throw std::runtime_error("Invalid dataset specified in query");
            }
            datasets_to_query.emplace_back(dsptr);
        }
    }

    std::unordered_set<IndexType> types_to_query;
    for (const auto *ds : datasets_to_query) {
        for (const auto &ndx : ds->get_indexes()) {
            types_to_query.emplace(ndx.index_type());
        }
    }

    const QueryGraphCollection graphs{query, types_to_query, config};

    task->spec().estimate_work(datasets_to_query.size());

    QueryCounters counters;
    for (const auto *ds : datasets_to_query) {
        task->spec().add_progress(1);
        if (!ds->has_all_taints(taints)) {
            continue;
        }
        ds->execute(graphs, out, &counters);
    }
    return counters;
}

std::vector<std::string> DatabaseSnapshot::compact_smart_candidates() const {
    return find_compact_candidate(/*smart=*/true);
}

std::vector<std::string> DatabaseSnapshot::compact_full_candidates() const {
    return find_compact_candidate(/*smart=*/false);
}

bool is_dataset_locked(const std::unordered_map<uint64_t, TaskSpec> &tasks,
                       std::string_view dataset_id) {
    for (const auto &[k, task] : tasks) {
        if (task.has_lock(DatasetLock(dataset_id))) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> DatabaseSnapshot::find_compact_candidate(
    bool smart) const {
    const uint64_t max_datasets = config.get(ConfigKey::merge_max_datasets());
    const uint64_t max_files = config.get(ConfigKey::merge_max_files());

    // Try to find a best compact candidate. As a rating function, we use
    // "number of datasets" - "average number of files", because we want to
    // maximise number of datasets being compacted and minimise number of
    // files being compacted (we want to compact small datasets first).
    std::vector<const OnDiskDataset *> best_compact;
    int64_t best_compact_value = std::numeric_limits<int64_t>::min();
    for (auto &set : OnDiskDataset::get_compatible_datasets(datasets)) {
        // Check every set of compatible datasets.
        std::vector<const OnDiskDataset *> candidates;
        if (smart) {
            // When we're trying to be smart, ignore too small sets.
            candidates = std::move(OnDiskDataset::get_compact_candidates(set));
        } else {
            candidates = std::move(set);
        }

        // Ignore datasets that are locked.
        std::vector<const OnDiskDataset *> ready_candidates;
        for (const auto &candidate : candidates) {
            if (!is_dataset_locked(tasks, candidate->get_id())) {
                ready_candidates.push_back(candidate);
            }
        }

        // Compute number of files in this dataset.
        uint64_t number_of_files = 0;
        for (const auto &candidate : ready_candidates) {
            number_of_files += candidate->get_file_count();
        }

        // Check for allowed maximum merge values. If they are exceeded, try to
        // merge small datasets first.
        std::sort(ready_candidates.begin(), ready_candidates.end(),
                  [](const auto &lhs, const auto &rhs) {
                      return lhs->get_file_count() < rhs->get_file_count();
                  });
        while (ready_candidates.size() > max_datasets ||
               number_of_files > max_files) {
            number_of_files -= ready_candidates.back()->get_file_count();
            ready_candidates.pop_back();
        }

        // It doesn't make sense to merge less than 2 datasets.
        if (ready_candidates.size() < 2) {
            continue;
        }

        // Compute compact_value for this set, and maybe update best candidate.
        uint64_t avg_files = number_of_files / ready_candidates.size();
        int64_t compact_value = ready_candidates.size() - avg_files;
        if (compact_value > best_compact_value) {
            best_compact_value = compact_value;
            best_compact = std::move(ready_candidates);
        }
    }

    if (best_compact.empty()) {
        spdlog::debug("No suitable compact candidate found.");
    } else {
        spdlog::debug("Good candidate (cost: {}, datasets: {}).",
                      best_compact_value, best_compact.size());
    }

    std::vector<std::string> names;
    names.reserve(best_compact.size());
    for (const auto &ds : best_compact) {
        names.emplace_back(ds->get_id());
    }

    return names;
}

void DatabaseSnapshot::compact_locked_datasets(Task *task) const {
    std::vector<const OnDiskDataset *> datasets;
    for (const auto &lock : task->spec().locks()) {
        if (const auto *dslock = std::get_if<DatasetLock>(&lock)) {
            const OnDiskDataset *ds = find_dataset(dslock->target());
            if (ds == nullptr) {
                throw std::runtime_error("Locked DS doesn't exist");
            }
            datasets.push_back(ds);
        }
    }
    internal_compact(task, std::move(datasets));
}

// Do some plumbing necessary to pass the data to OnDiskDataset::merge.
// After the merging, do more plumbing to add results to the task->changes
// collection.
void DatabaseSnapshot::internal_compact(
    Task *task, std::vector<const OnDiskDataset *> datasets) const {
    std::vector<std::string> ds_names;

    // There's nothing to compact
    if (datasets.size() < 2) {
        return;
    }

    ds_names.reserve(datasets.size());
    for (const auto *ds : datasets) {
        ds_names.push_back(ds->get_name());
    }

    std::string dataset_name = allocate_name().get_filename();
    OnDiskDataset::merge(db_base, dataset_name, datasets, &task->spec());

    for (auto &dataset : datasets) {
        task->change(DBChange(DbChangeType::Drop, dataset->get_id()));
    }

    task->change(DBChange(DbChangeType::Insert, dataset_name));
}
