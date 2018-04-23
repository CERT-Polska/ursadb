#include "Database.h"

#include <experimental/filesystem>
#include <fstream>
#include <iostream>

#include "ExclusiveFile.h"
#include "lib/Json.h"

using json = nlohmann::json;

Database::Database() : max_memory_size(DEFAULT_MAX_MEM_SIZE), num_datasets(0) {}

Database::Database(const std::string &fname) : db_fname(fname) {
    std::ifstream db_file(db_fname);
    json db_json;
    db_file >> db_json;

    num_datasets = db_json["num_datasets"];
    max_memory_size = db_json["max_mem_size"];

    for (std::string dataset_fname : db_json["datasets"]) {
        datasets.emplace_back(dataset_fname);
    }
}

void Database::create(const std::string &fname) {
    ExclusiveFile lock(fname);
    if (!lock.is_ok()) {
        // TODO() implement either-type error class
        throw std::runtime_error("File already exists");
    }
    Database empty;
    empty.db_fname = fname;
    empty.save();
}

std::string Database::allocate_name() {
    while (true) {
        // TODO limit this to some sane value (like 10000 etc),
        // to avoid infinite loop in exceptional cases.

        std::stringstream ss;
        ss << "set." << num_datasets << "." << db_fname;
        num_datasets++;
        std::string fname = ss.str();
        ExclusiveFile lock(fname);
        if (lock.is_ok()) {
            return fname;
        }
    }
}

void Database::add_dataset(DatasetBuilder &builder) {
    auto dataset_name = allocate_name();
    builder.save(dataset_name);
    datasets.emplace_back(dataset_name);
}

void Database::compact() {
    std::string dataset_name = allocate_name();
    OnDiskDataset::merge(dataset_name, datasets);

    for (auto &dataset : datasets) {
        dataset.drop();
    }

    datasets.clear();

    using json = nlohmann::json;
    datasets.emplace_back(dataset_name);
    save();
}

void Database::execute(const Query &query, std::vector<std::string> &out) {
    for (const auto &ds : datasets) {
        ds.execute(query, &out);
    }
}

void Database::save() {
    std::ofstream db_file(db_fname, std::ofstream::out);
    json db_json;
    db_json["num_datasets"] = num_datasets;
    db_json["max_mem_size"] = max_memory_size;
    std::vector<std::string> dataset_names;

    for (const auto &ds : datasets) {
        dataset_names.push_back(ds.get_name());
    }

    db_json["datasets"] = dataset_names;
    db_file << std::setw(4) << db_json << std::endl;
    db_file.close();
}

void Database::index_path(const std::vector<IndexType> types, const std::string &filepath) {
    namespace fs = std::experimental::filesystem;
    DatasetBuilder builder(types);
    fs::recursive_directory_iterator end;

    for (fs::recursive_directory_iterator dir(filepath); dir != end; ++dir) {
        if (fs::is_regular_file(dir->path())) {
            std::cout << dir->path().string() << std::endl;

            try {
                builder.index(dir->path().string());
            } catch (empty_file_error &e) {
                std::cout << "empty file, skip" << std::endl;
            }

            if (builder.estimated_size() > max_memory_size) {
                std::cout << "new dataset " << builder.estimated_size() << std::endl;
                add_dataset(builder);
                builder = DatasetBuilder(types);
            }
        }
    }

    add_dataset(builder);
    save();
}
