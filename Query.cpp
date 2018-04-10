#include "Query.h"
#include <utility>

#include "Utils.h"

const std::vector<Query> &Query::as_queries() const {
    return queries;
}

Query::Query(const TriGram &trigram)
        : type(QueryType::PRIMITIVE), trigram(trigram) {}

Query::Query(const QueryType &type, const std::vector<Query> &queries)
        : type(type), queries(std::move(queries)) {}

Query::Query(const std::string &str)
        : type(QueryType::AND), queries() {
    auto trigrams = get_trigrams(str, str.size());

    for (auto trigram : trigrams) {
        queries.emplace_back(trigram);
    }
}

void Query::print_query() const {
    if (type == QueryType::AND || type == QueryType::OR) {
        if (type == QueryType::AND) {
            std::cout << "AND(";
        } else if (type == QueryType::OR) {
            std::cout << "OR(";
        }

        bool is_first = true;
        for (const auto &q : queries) {
            if (!is_first) {
                std::cout << ", ";
            }
            is_first = false;
            q.print_query();
        }
        std::cout << ")";
    } else if (type == QueryType::PRIMITIVE) {
        std::cout << std::hex << "'" << trigram << "'";
    }
}

const QueryType &Query::get_type() const {
    return type;
}

const TriGram &Query::as_trigram() const {
    return trigram;
}

Query q(const std::string &str) {
    return Query(str);
}

Query q_and(const std::vector<Query> &queries) {
    return Query(QueryType::AND, queries);
}

Query q_or(const std::vector<Query> &queries) {
    return Query(QueryType::OR, queries);
}
