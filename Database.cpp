#include "Database.h"

#include <experimental/filesystem>
#include <fstream>
#include <iostream>

#include "ExclusiveFile.h"
#include "lib/Json.h"

using json = nlohmann::json;
namespace fs = std::experimental::filesystem;

Database::Database(const std::string &fname, bool initialize) : tasks(), last_task_id(0) {
    std::random_device rd;
    std::seed_seq seed{rd(), rd(), rd(), rd()}; // A bit better than pathetic default
    std::mt19937_64 gen(seed);

    db_name = fs::path(fname).filename();
    db_base = fs::path(fname).parent_path();

    if (initialize) {
        load_from_disk();
    } else {
        max_memory_size = DEFAULT_MAX_MEM_SIZE;
    }
}

Database::Database(const std::string &fname) : Database(fname, true) {
}

void Database::load_from_disk() {
    std::ifstream db_file(db_base / db_name, std::ifstream::binary);

    if (db_file.fail()) {
        throw std::runtime_error("Failed to open database file");
    }

    json db_json;

    try {
        db_file >> db_json;
    } catch (json::parse_error &e) {
        throw std::runtime_error("Failed to parse JSON");
    }

    // TODO(xmsm) - when not present, use default
    max_memory_size = db_json["config"]["max_mem_size"];

    for (std::string dataset_fname : db_json["datasets"]) {
        datasets.emplace_back(db_base, dataset_fname);
    }
}

void Database::create(const std::string &fname) {
    ExclusiveFile lock(fname);
    if (!lock.is_ok()) {
        // TODO() implement either-type error class
        throw std::runtime_error("File already exists");
    }
    Database empty(fname, false);
    empty.save();
}

std::string random_hex_string(int length, std::mt19937_64 *rd) {
    constexpr static char charset[] = "0123456789abcdef";
    thread_local static std::uniform_int_distribution<int> pick(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);

    for (int i = 0; i < length; i++) {
        result += charset[pick(*rd)];
    }

    return result;
}

std::string Database::allocate_name() {
    while (true) {
        // TODO limit this to some sane value (like 10000 etc),
        // to avoid infinite loop in exceptional cases.

        std::stringstream ss;
        ss << "set." << random_hex_string(8, &random) << "." << db_name.string();
        std::string fname = ss.str();
        ExclusiveFile lock(db_base / fname);
        if (lock.is_ok()) {
            return fname;
        }
    }
}

uint64_t Database::allocate_task_id() {
    // TODO data race
    return ++last_task_id;
}

Task *Database::allocate_task() {
    while (true) {
        uint64_t task_id = allocate_task_id();
        auto timestamp = std::chrono::steady_clock::now().time_since_epoch();
        uint64_t epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count();
        if (tasks.count(task_id) == 0) {
            Task new_task(task_id, epoch_ms);
            return &tasks.emplace(task_id, new_task).first->second;
        }
    }
}

std::string Database::add_dataset(DatasetBuilder &builder) {
    auto dataset_name = allocate_name();
    builder.save(db_base, dataset_name);
    return dataset_name;
    /* OnDiskDataset &ds = datasets.emplace_back(db_base, dataset_name);

    for (auto &fn : ds.indexed_files()) {
        all_files.insert(fn);
    } */
}

void Database::compact(Task *task) {
    std::string dataset_name = allocate_name();
    OnDiskDataset::merge(db_base, dataset_name, datasets, task);

    for (auto &dataset : datasets) {
        task->changes.emplace_back("drop", dataset.get_name());
    }

    task->changes.emplace_back("insert", dataset_name);
}

void Database::execute(const Query &query, Task *task, std::vector<std::string> *out) {
    task->work_estimated = datasets.size();

    for (const auto &ds : datasets) {
        ds.execute(query, out);
        task->work_done += 1;
    }
}

void Database::save() {
    std::ofstream db_file(db_base / db_name, std::ofstream::out | std::ofstream::binary);
    json db_json;
    db_json["config"] = {
        {"max_mem_size", max_memory_size}
    };
    std::vector<std::string> dataset_names;

    for (const auto &ds : datasets) {
        dataset_names.push_back(ds.get_name());
    }

    db_json["datasets"] = dataset_names;
    db_file << std::setw(4) << db_json << std::endl;
}

void Database::index_path(Task *task, const std::vector<IndexType> types, const std::string &filepath) {
    namespace fs = std::experimental::filesystem;
    DatasetBuilder builder(types);
    fs::recursive_directory_iterator end;

    if (all_files.empty()) {
        for (auto &dataset : datasets) {
            for (auto &fn : dataset.indexed_files()) {
                all_files.insert(fn);
            }
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

        if (builder.estimated_size() > max_memory_size) {
            std::cout << "new dataset " << builder.estimated_size() << std::endl;
            task->changes.emplace_back("insert", add_dataset(builder));
            builder = DatasetBuilder(types);
        }

        task->work_done += 1;
    }

    if (!builder.empty()) {
        task->changes.emplace_back("insert", add_dataset(builder));
    }

    task->work_done += 1;
}
