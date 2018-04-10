#include "Database.h"


Database::Database(const std::string &fname) : db_fname(fname) {
    std::ifstream in(fname);
    json j;
    in >> j;

    num_datasets = j["num_datasets"];

    for (std::string dataset_fname : j["datasets"]) {
        datasets.emplace_back(dataset_fname);
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

std::vector<std::string> intersection(std::vector<std::string> &v1, std::vector<std::string> &v2)
{
    std::vector<std::string> v3;

    sort(v1.begin(), v1.end());
    sort(v2.begin(), v2.end());

    set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(), back_inserter(v3));

    return v3;
}

void Database::execute(const Query &query, std::vector<std::string> &out) {
    // TODO(monk): needs reconsideration but it's around 18:00 and Rev is forcing me to quit for today

    if (query.get_type() == QueryType::PRIMITIVE) {
        for (auto &dataset : datasets) {
            dataset.query_primitive(query.as_trigram(), out);
        }
    } else if (query.get_type() == QueryType::OR) {
        std::vector<std::string> partial;

        for (auto &q : query.as_queries()) {
            execute(q, partial);

            for (std::string &v : partial) {
                out.push_back(v);
            }
        }
    } else if (query.get_type() == QueryType::AND) {
        std::vector<std::string> partial;
        bool is_first = true;

        for (auto &q : query.as_queries()) {
            execute(q, partial);

            if (!is_first) {
                out = intersection(out, partial);
            } else {
                out = partial;
            }

            is_first = false;
        }
    }
}

//void Database::index_path(const std::string &path) {
    // TODO allocate DatasetBuilder
    // index_file(builder, "test.txt");
    // builder.save("dataset.ursa");
    // compact
//}
