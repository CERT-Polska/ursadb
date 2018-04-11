#include "Query.h"

void QueryResult::do_or(const QueryResult &&other) {
    if (this->is_everything() || other.is_everything()) {
        has_everything = true;
        return;
    }

    std::vector<FileId> new_results;
    std::set_union(
        other.results.begin(), other.results.end(),
        results.begin(), results.end(),
        std::back_inserter(new_results));
    std::swap(new_results, results);
}

void QueryResult::do_and(const QueryResult &&other) {
    if (other.is_everything()) {
        return;
    }
    if (this->is_everything()) {
        *this = QueryResult(other);
        return;
    }

    auto new_end = std::set_intersection(
        other.results.begin(), other.results.end(),
        results.begin(), results.end(),
        results.begin());
    results.erase(new_end, results.end());
}

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

std::ostream& operator<<(std::ostream& os, const Query& query) {
    QueryType type = query.get_type();
    if (type == QueryType::AND || type == QueryType::OR) {
        if (type == QueryType::AND) {
            os << "AND(";
        } else if (type == QueryType::OR) {
            os << "OR(";
        }

        bool is_first = true;
        for (const auto &q : query.as_queries()) {
            if (!is_first) {
                os << ", ";
            }
            is_first = false;
            os << q;
        }
        os << ")";
    } else if (type == QueryType::PRIMITIVE) {
        os << std::hex << "'" << query.as_trigram() << "'";
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
