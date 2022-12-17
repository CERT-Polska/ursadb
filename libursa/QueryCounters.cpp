#include "QueryCounters.h"

#include <algorithm>

void QueryCounter::add(const QueryCounter &other) {
    count_ += other.count_;
    duration_ += other.duration_;
}

void QueryCounters::add(const QueryCounters &other) {
    ors_.add(other.ors_);
    ands_.add(other.ands_);
    reads_.add(other.reads_);
    uniq_reads_.add(other.uniq_reads_);
    minofs_.add(other.minofs_);
}

std::unordered_map<std::string, QueryCounter> QueryCounters::counters() const {
    std::unordered_map<std::string, QueryCounter> result;
    result["or"] = ors_;
    result["and"] = ands_;
    result["read"] = reads_;
    result["uniq_read"] = uniq_reads_;
    result["minof"] = minofs_;
    return result;
}

QueryOperation::~QueryOperation() {
    auto duration = std::chrono::steady_clock::now() - start_;
    parent->add(QueryCounter(1, duration));
}
