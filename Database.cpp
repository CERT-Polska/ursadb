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

void Database::add_dataset(DatasetBuilder &builder) {
    std::stringstream ss;
    ss << "set." << num_datasets << "." << db_fname;
    builder.save(ss.str());
    datasets.emplace_back(ss.str());
    num_datasets++;
}

void Database::compact() {
    // TODO(monk): compact, rewrite DB to the optimal form
}

void Database::execute(const Query &query, std::vector<std::string> &out) {
    for (const auto &ds: datasets) {
        ds.execute(query, &out);
    }
}

void Database::save() {
    std::ofstream db_file(db_fname);
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
