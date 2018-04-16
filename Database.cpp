#include <cstdio>
#include <boost/filesystem.hpp>

#include "Database.h"


Database::Database(const std::string &fname) : db_fname(fname) {
    std::ifstream db_file(fname);

    if (db_file.fail()) {
        num_datasets = 0;
    } else {
        json db_json;
        db_file >> db_json;

        num_datasets = db_json["num_datasets"];

        for (std::string dataset_fname : db_json["datasets"]) {
            datasets.emplace_back(dataset_fname);
        }

        db_file.close();
    }
}

std::string Database::allocate_name() {
    std::stringstream ss;
    ss << "set." << num_datasets << "." << db_fname;
    num_datasets++;
    return ss.str();
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
    datasets.emplace_back(dataset_name);
    save();
}

void Database::execute(const Query &query, std::vector<std::string> &out) {
    for (const auto &ds: datasets) {
        ds.execute(query, &out);
    }
}

void Database::save() {
    std::ofstream db_file(db_fname, std::ofstream::out);
    json db_json;
    db_json["num_datasets"] = num_datasets;
    std::vector<std::string> dataset_names;

    for (const auto &ds : datasets) {
        dataset_names.push_back(ds.get_name());
    }

    db_json["datasets"] = dataset_names;
    db_file << std::setw(4) << db_json << std::endl;
    db_file.close();
}

void Database::index_path(const std::string &filepath) {
    using namespace boost::filesystem;

    DatasetBuilder builder;
    recursive_directory_iterator end;

    for (recursive_directory_iterator dir(filepath); dir != end; ++dir)
    {
        if (is_regular_file(dir->path())) {
            std::cout << dir->path().string() << std::endl;

            try {
                builder.index(dir->path().string());
            } catch (empty_file_error &e) {
                std::cout << "empty file, skip" << std::endl;
            }

            if (builder.processed_bytes() > 1024L*1024*512) {
                std::cout << "new dataset " << builder.processed_bytes() << std::endl;
                add_dataset(builder);
                builder = DatasetBuilder();
            }
        }
    }

    add_dataset(builder);
    save();
}
