#include "QueryResult.h"

#include <algorithm>

void QueryResult::do_or(const QueryResult &other, QueryCounter *counter) {
    auto op = QueryOperation(counter);
    if (this->is_everything() || other.is_everything()) {
        has_everything = true;
        results = SortedRun();
    } else {
        results.do_or(other.results);
    }
}

void QueryResult::do_and(const QueryResult &other, QueryCounter *counter) {
    auto op = QueryOperation(counter);
    if (other.is_everything()) {
    } else if (this->is_everything()) {
        results = other.results;
        has_everything = other.has_everything;
    } else {
        results.do_and(other.results);
    }
}

QueryResult QueryResult::do_min_of_real(
    int cutoff, const std::vector<const QueryResult *> &sources) {
    std::vector<const SortedRun *> nontrivial_sources;
    for (const auto *source : sources) {
        if (source->is_everything()) {
            cutoff -= 1;
        } else if (!source->is_empty()) {
            nontrivial_sources.push_back(&source->vector());
        }
    }

    // '0 of (...)' should match everything.
    if (cutoff <= 0) {
        return QueryResult::everything();
    }

    // Short circuit when cutoff is too big.
    // This may happen when there are `everything` results in sources.
    if (cutoff > static_cast<int>(nontrivial_sources.size())) {
        return QueryResult::empty();
    }

    // Special case optimisation for cutoff==1 and a single source.
    if (cutoff == 1 && nontrivial_sources.size() == 1) {
        return QueryResult(nontrivial_sources[0]->clone());
    }

    // Special case optimisation - reduction to AND.
    if (cutoff == static_cast<int>(nontrivial_sources.size())) {
        QueryResult out{QueryResult::everything()};
        for (const auto &src : nontrivial_sources) {
            out.results.do_and(*src);
        }
        return out;
    }

    // Special case optimisation - reduction to OR.
    if (cutoff == 1) {
        QueryResult out{QueryResult::empty()};
        for (const auto &src : nontrivial_sources) {
            out.results.do_or(*src);
        }
        return out;
    }

    return QueryResult(SortedRun::pick_common(cutoff, nontrivial_sources));
}

QueryResult QueryResult::do_min_of(
    int cutoff, const std::vector<const QueryResult *> &sources,
    QueryCounter *counter) {
    QueryOperation op(counter);
    QueryResult out{do_min_of_real(cutoff, sources)};
    return out;
}
