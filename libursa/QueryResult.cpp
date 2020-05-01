#include "QueryResult.h"

#include <algorithm>

void QueryResult::do_or(const QueryResult &other, QueryStatistics *toupdate) {
    QueryOperation op(file_count() + other.file_count());
    if (this->is_everything() || other.is_everything()) {
        has_everything = true;
        results.clear();
    } else {
        std::vector<FileId> new_results;
        std::set_union(other.results.begin(), other.results.end(),
                       results.begin(), results.end(),
                       std::back_inserter(new_results));
        std::swap(new_results, results);
    }
    stats_.add(other.stats_);
    toupdate->add_or(op.end(file_count()));
}

void QueryResult::do_or(const QueryResult &other) { do_or(other, &stats_); }

void QueryResult::do_and(const QueryResult &other, QueryStatistics *toupdate) {
    QueryOperation op(file_count() + other.file_count());
    if (other.is_everything()) {
    } else if (this->is_everything()) {
        results = other.results;
        has_everything = other.has_everything;
    } else {
        auto new_end = std::set_intersection(
            other.results.begin(), other.results.end(), results.begin(),
            results.end(), results.begin());
        results.erase(new_end, results.end());
    }
    stats_.add(other.stats_);
    toupdate->add_and(op.end(file_count()));
}

void QueryResult::do_and(const QueryResult &other) { do_and(other, &stats_); }

QueryCounter QueryOperation::end(uint32_t out_files) const {
    auto duration = std::chrono::steady_clock::now() - start_;
    return QueryCounter(1, in_files_, out_files, duration);
}

void QueryCounter::add(const QueryCounter &other) {
    count_ += other.count_;
    in_files_ += other.in_files_;
    out_files_ += other.out_files_;
    duration_ += other.duration_;
}

void QueryStatistics::add(const QueryStatistics &other) {
    ors_.add(other.ors_);
    ands_.add(other.ands_);
    reads_.add(other.reads_);
}

std::unordered_map<std::string, QueryCounter> QueryStatistics::counters()
    const {
    std::unordered_map<std::string, QueryCounter> result;
    result["or"] = ors_;
    result["and"] = ands_;
    result["read"] = reads_;
    return result;
}
