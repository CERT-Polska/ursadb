#include "DatabaseSnapshot.h"

#include <fstream>

#include "Database.h"
#include "DatasetBuilder.h"
#include "ExclusiveFile.h"
#include "Json.h"

std::string random_hex_string(int length) {
    constexpr static char charset[] = "0123456789abcdef";
    thread_local static std::random_device rd;
    thread_local static std::seed_seq seed{rd(), rd(), rd(), rd()}; // A bit better than pathetic default
    thread_local static std::mt19937_64 random(seed);
    thread_local static std::uniform_int_distribution<int> pick(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);

    for (int i = 0; i < length; i++) {
        result += charset[pick(random)];
    }

    return result;
}

DatabaseSnapshot::DatabaseSnapshot(
        fs::path db_name, fs::path db_base, std::vector<const OnDiskDataset *> datasets,
        const std::map<uint64_t, std::unique_ptr<Task>> &tasks, size_t max_memory_size)
    : db_name(db_name), db_base(db_base), datasets(datasets), tasks(),
      max_memory_size(max_memory_size) {
    for (const auto &entry : tasks) {
        this->tasks.emplace(entry.first, *entry.second.get());
    }
}

void DatabaseSnapshot::index_path(
        Task *task, const std::vector<IndexType> types, const std::string &filepath) const {
    DatasetBuilder builder(types);
    fs::recursive_directory_iterator end;
    std::set<std::string> all_files;

    for (auto &dataset : datasets) {
        for (auto &fn : dataset->indexed_files()) {
            all_files.insert(fn);
        }
    }

    std::vector<std::string> targets;

    for (fs::recursive_directory_iterator dir(filepath); dir != end; ++dir) {
        if (fs::is_regular_file(dir->path())) {
            fs::path absfn = fs::absolute(dir->path());

            if (all_files.find(absfn.string()) != all_files.end()) {
                continue;
            }

            targets.push_back(absfn.string());
        }
    }

    task->work_estimated = targets.size() + 1;

    for (const auto &target : targets) {
        std::cout << "indexing " << target << std::endl;

        try {
            builder.index(target);
        } catch (empty_file_error &e) {
            std::cout << "empty file, skip" << std::endl;
        }

        if (builder.must_spill()) {
            std::cout << "new dataset" << std::endl;
            auto dataset_name = allocate_name();
            builder.save(db_base, dataset_name);
            task->changes.emplace_back(DbChangeType::Insert, dataset_name);
            builder = DatasetBuilder(types);
        }

        task->work_done += 1;
    }

    if (!builder.empty()) {
        auto dataset_name = allocate_name();
        builder.save(db_base, dataset_name);
        task->changes.emplace_back(DbChangeType::Insert, dataset_name);
    }

    task->work_done += 1;
}

void DatabaseSnapshot::reindex_dataset(
        Task *task, const std::vector<IndexType> types, const std::string &dataset_name) const {
    const OnDiskDataset *source = nullptr;

    for (const auto *ds : datasets) {
        if (ds->get_id() == dataset_name) {
            source = ds;
        }
    }

    if (source == nullptr) {
        throw std::runtime_error("source dataset was not found");
    }

    double work_done_f = 0;
    double work_increment = (double)(types.size() * NUM_TRIGRAMS) / source->indexed_files().size();
    task->work_estimated = 2 * types.size() * NUM_TRIGRAMS + 1;

    std::vector<OnDiskDataset> compactTargets;
    std::vector<const OnDiskDataset*> compactTargetsPtr;
    std::string reindexPrefix = random_hex_string(8);
    DatasetBuilder builder(types);

    for (const std::string &fname : source->indexed_files()) {
        std::cout << "reindexing " << fname << std::endl;

        try {
            builder.index(fname);
        } catch (empty_file_error &e) {
            std::cout << "empty file, skip" << std::endl;
        }

        if (builder.must_spill()) {
            std::stringstream ss;
            ss << "reindex." << reindexPrefix << "." << compactTargets.size() << ".ursa";
            builder.save(db_base, ss.str());
            compactTargets.emplace_back(db_base, ss.str());
            builder = DatasetBuilder(types);
        }

        work_done_f += work_increment;
        task->work_done = (uint64_t)work_done_f;
    }

    task->work_done = types.size() * NUM_TRIGRAMS;

    if (!builder.empty()) {
        std::stringstream ss;
        ss << "reindex." << reindexPrefix << "." << compactTargets.size() << ".ursa";
        builder.save(db_base, ss.str());
        compactTargets.emplace_back(db_base, ss.str());
    }

    for (const auto &ds : compactTargets) {
        compactTargetsPtr.push_back(&ds);
    }

    std::string mainTarget;

    if (compactTargets.size() > 1) {
        std::stringstream ss;
        ss << "reindex." << reindexPrefix << ".merged.ursa";
        mainTarget = ss.str();
        OnDiskDataset::merge(db_base, mainTarget, compactTargetsPtr, task);

        for (auto &ds : compactTargets) {
            ds.drop();
        }
    } else if (compactTargets.size() == 1) {
        // turns out that merge will be not needed, progress boost
        task->work_done += types.size() * NUM_TRIGRAMS;
        mainTarget = compactTargets[0].get_name();
    } else {
        throw std::runtime_error("nothing to reindex");
    }

    {
        OnDiskDataset target_ds(db_base, mainTarget);

        if (target_ds.indexed_files() != source->indexed_files()) {
            throw std::runtime_error("reindex produced faulty dataset, file list doesn\'t match with the source");
        }
    }

    std::ifstream in(db_base / source->get_name(), std::ifstream::binary);
    json j;
    in >> j;
    in.close();

    for (const auto &type : types) {
        fs::path old_index_name = db_base / (get_index_type_name(type) + "." + mainTarget);
        fs::path new_index_name = db_base / (get_index_type_name(type) + "." + source->get_name());
        fs::rename(old_index_name, new_index_name);
        j["indices"].emplace_back(get_index_type_name(type) + "." + source->get_name());
    }

    std::ofstream out(db_base / source->get_name(), std::ofstream::binary);
    out << std::setw(4) << j;
    out.close();

    fs::remove(db_base / ("files." + mainTarget));
    fs::remove(db_base / mainTarget);

    task->changes.emplace_back(DbChangeType::Reload, source->get_name());

    task->work_done += 1;
}

std::string DatabaseSnapshot::allocate_name() const {
    while (true) {
        // TODO limit this to some sane value (like 10000 etc),
        // to avoid infinite loop in exceptional cases.

        std::stringstream ss;
        ss << "set." << random_hex_string(8) << "." << db_name.string();
        std::string fname = ss.str();
        ExclusiveFile lock(db_base / fname);
        if (lock.is_ok()) {
            return fname;
        }
    }
}

void DatabaseSnapshot::execute(const Query &query, Task *task, std::vector<std::string> *out) const {
    task->work_estimated = datasets.size();

    for (const auto &ds : datasets) {
        ds->execute(query, out);
        task->work_done += 1;
    }
}

void DatabaseSnapshot::compact(Task *task) const {
    std::string dataset_name = allocate_name();
    OnDiskDataset::merge(db_base, dataset_name, datasets, task);

    for (auto &dataset : datasets) {
        task->changes.emplace_back(DbChangeType::Drop, dataset->get_name());
    }

    task->changes.emplace_back(DbChangeType::Insert, dataset_name);
}
