#include "Database.h"

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <memory>

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

    for (const std::string &dataset_fname : db_json["datasets"]) {
        load_dataset(dataset_fname);
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

void Database::save() {
    std::ofstream db_file(db_base / db_name, std::ofstream::out | std::ofstream::binary);
    json db_json;
    db_json["config"] = {
        {"max_mem_size", max_memory_size}
    };
    std::vector<std::string> dataset_names;

    for (const auto *ds : working_datasets) {
        dataset_names.push_back(ds->get_name());
    }

    db_json["datasets"] = dataset_names;
    db_file << std::setw(4) << db_json << std::endl;
}

void Database::load_dataset(const std::string &ds) {
    loaded_datasets.push_back(std::make_unique<OnDiskDataset>(db_base, ds));
    working_datasets.push_back(loaded_datasets.back().get());
    std::cout << "loaded new dataset " << ds << std::endl;
}

void Database::drop_dataset(const std::string &dsname) {
    for (auto it = working_datasets.begin(); it != working_datasets.end();) {
        if ((*it)->get_name() == dsname) {
            it = working_datasets.erase(it);
            std::cout << "drop " << dsname << std::endl;
        } else {
            ++it;
        }
    }
}

void Database::unload_dataset(const std::string &dsname) {
    for (auto it = loaded_datasets.begin(); it != loaded_datasets.end();) {
        if ((*it).get()->get_name() == dsname) {
            // TODO delete dataset from fs
            it = loaded_datasets.erase(it);
            std::cout << "unload ds " << dsname << std::endl;
        } else {
            ++it;
        }
    }
}
