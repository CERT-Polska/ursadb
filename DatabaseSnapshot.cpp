#include "DatabaseSnapshot.h"

#include "Database.h"
#include "DatasetBuilder.h"
#include "ExclusiveFile.h"

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
    namespace fs = std::experimental::filesystem;
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
