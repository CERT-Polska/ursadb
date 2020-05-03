#include "QueryResult.h"

#include <algorithm>

void QueryResult::do_or_real(const std::vector<FileId> &other) {
    std::vector<FileId> new_results;
    std::set_union(other.begin(), other.end(), results.begin(), results.end(),
                   std::back_inserter(new_results));
    std::swap(new_results, results);
}

void QueryResult::do_or(const QueryResult &other, QueryStatistics *toupdate) {
    QueryOperation op(file_count() + other.file_count());
    if (this->is_everything() || other.is_everything()) {
        has_everything = true;
        results.clear();
    } else {
        do_or_real(other.results);
    }
    stats_.add(other.stats_);
    toupdate->add_or(op.end(file_count()));
}

void QueryResult::do_or(const QueryResult &other) { do_or(other, &stats_); }

void QueryResult::do_and_real(const std::vector<FileId> &other) {
    auto new_end =
        std::set_intersection(other.begin(), other.end(), results.begin(),
                              results.end(), results.begin());
    results.erase(new_end, results.end());
}

void QueryResult::do_and(const QueryResult &other, QueryStatistics *toupdate) {
    QueryOperation op(file_count() + other.file_count());
    if (other.is_everything()) {
    } else if (this->is_everything()) {
        results = other.results;
        has_everything = other.has_everything;
    } else {
        do_and_real(other.results);
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
    minofs_.add(other.minofs_);
}

std::unordered_map<std::string, QueryCounter> QueryStatistics::counters()
    const {
    std::unordered_map<std::string, QueryCounter> result;
    result["or"] = ors_;
    result["and"] = ands_;
    result["read"] = reads_;
    result["minof"] = minofs_;
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
                min_index =
                    i;  // TODO(unknown): benchmark and consider removing.
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

QueryResult QueryResult::do_min_of_real(
    int cutoff, const std::vector<const QueryResult *> &sources) {
    std::vector<const std::vector<FileId> *> nontrivial_sources;
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
        return QueryResult(std::vector<FileId>(*nontrivial_sources[0]));
    }

    // Special case optimisation - reduction to AND.
    if (cutoff == static_cast<int>(nontrivial_sources.size())) {
        QueryResult out{QueryResult::everything()};
        for (const auto &src : nontrivial_sources) {
            out.do_and_real(*src);
        }
        return out;
    }

    // Special case optimisation - reduction to OR.
    if (cutoff == 1) {
        QueryResult out{QueryResult::empty()};
        for (const auto &src : nontrivial_sources) {
            out.do_or_real(*src);
        }
        return out;
    }

    return QueryResult(internal_pick_common(cutoff, nontrivial_sources));
}

QueryResult QueryResult::do_min_of(
    int cutoff, const std::vector<const QueryResult *> &sources,
    QueryStatistics *toupdate) {
    uint64_t total_files = 0;
    for (const auto *src : sources) {
        total_files += src->file_count();
    }
    QueryOperation op(total_files);
    QueryResult out{do_min_of_real(cutoff, sources)};
    toupdate->add_minof(op.end(out.file_count()));
    return out;
}
