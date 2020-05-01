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

std::vector<FileId> internal_pick_common(
    int cutoff, const std::vector<const std::vector<FileId> *> &sources) {
    // returns all FileIds which appear at least `cutoff` times among provided
    // `sources`
    using FileIdRange = std::pair<std::vector<FileId>::const_iterator,
                                  std::vector<FileId>::const_iterator>;
    std::vector<FileId> result;
    std::vector<FileIdRange> heads;
    heads.reserve(sources.size());

    for (auto source : sources) {
        if (!source->empty()) {
            heads.emplace_back(
                std::make_pair(source->cbegin(), source->cend()));
        }
    }

    while (static_cast<int>(heads.size()) >= cutoff) {
        // pick lowest possible FileId value among all current heads
        int min_index = 0;
        FileId min_id = *heads[0].first;
        for (int i = 1; i < static_cast<int>(heads.size()); i++) {
            if (*heads[i].first < min_id) {
                min_index = i;  // TODO benchmark and consider removing.
                min_id = *heads[i].first;
            }
        }

        // fix on that particular value selected in previous step and count
        // number of repetitions among heads.
        // Note that it's implementation-defined that std::vector<FileId>
        // is always sorted and we use this fact here.
        int repeat_count = 0;
        for (int i = min_index; i < static_cast<int>(heads.size()); i++) {
            if (*heads[i].first == min_id) {
                repeat_count += 1;
                heads[i].first++;
                // head ended, we may get rid of it
                if (heads[i].first == heads[i].second) {
                    heads.erase(heads.begin() + i);
                    i--;  // Be careful not to skip elements!
                }
            }
        }

        // this value has enough repetitions among different heads to add it to
        // the result set
        if (repeat_count >= cutoff) {
            result.push_back(min_id);
        }
    }

    return result;
}

QueryResult QueryResult::do_min_of(
    int cutoff, const std::vector<const QueryResult *> &sources) {
    if (cutoff > static_cast<int>(sources.size())) {
        // Short circuit when cutoff is too big.
        // This should never happen for well-formed queries, but this check is
        // very cheap.
        return QueryResult::empty();
    }
    if (cutoff <= 0) {
        // '0 of (...)' should match everything.
        return QueryResult::everything();
    }

    std::vector<const std::vector<FileId> *> nontrivial_sources;
    for (const auto *source : sources) {
        if (source->is_everything()) {
            cutoff -= 1;
            if (cutoff <= 0) {
                // Short circuit when result is trivially everything().
                return QueryResult::everything();
            }
        } else if (!source->is_empty()) {
            nontrivial_sources.push_back(&source->vector());
        }
    }

    // Special case optimization for cutoff==1 and a single source.
    if (cutoff == 1 && nontrivial_sources.size() == 1) {
        return QueryResult(std::vector<FileId>(*nontrivial_sources[0]));
    }

    return QueryResult(internal_pick_common(cutoff, nontrivial_sources));
}
