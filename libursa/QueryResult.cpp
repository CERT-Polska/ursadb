#include "QueryResult.h"

void QueryResult::do_or(const QueryResult &other) {
    if (this->is_everything() || other.is_everything()) {
        has_everything = true;
        return;
    }

    std::vector<FileId> new_results;
    std::set_union(other.results.begin(), other.results.end(), results.begin(),
                   results.end(), std::back_inserter(new_results));
    std::swap(new_results, results);
}

void QueryResult::do_and(const QueryResult &other) {
    if (other.is_everything()) {
        return;
    }
    if (this->is_everything()) {
        results = other.results;
        has_everything = other.has_everything;
        return;
    }

    auto new_end =
        std::set_intersection(other.results.begin(), other.results.end(),
                              results.begin(), results.end(), results.begin());
    results.erase(new_end, results.end());
}
