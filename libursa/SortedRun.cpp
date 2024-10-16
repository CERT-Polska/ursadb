#include "SortedRun.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

#include "Utils.h"

// Read element currently under pos.
uint32_t run_read(uint8_t *pos) {
    uint64_t acc = 0;
    uint32_t shift = 0;
    for (uint8_t *it = pos;; it++) {
        uint32_t next = *it;
        acc += (next & 0x7FU) << shift;
        shift += 7U;
        if ((next & 0x80U) == 0) {
            return acc + 1;
        }
    }
}

// Move pos to the next element.
uint8_t *run_forward(uint8_t *pos) {
    for (;; pos++) {
        if ((*pos & 0x80) == 0) {
            return pos + 1;
        }
    }
}

void SortedRun::validate_compression(bool expected) {
    if (!empty() && is_compressed() != expected) {
        throw std::runtime_error("Run was in invalid compression state");
    }
}

std::vector<uint32_t>::iterator SortedRun::begin() {
    validate_compression(false);
    return sequence_.begin();
}

std::vector<uint32_t>::iterator SortedRun::end() {
    validate_compression(false);
    return sequence_.end();
}

void SortedRun::do_or(SortedRun &other) {
    // In almost every case this is already decompressed.
    decompress();
    other.decompress();
    std::vector<FileId> new_results;
    std::set_union(other.begin(), other.end(), begin(), end(),
                    std::back_inserter(new_results));
    std::swap(new_results, sequence_);
}

// Read VLE integer under run_it_ and do the intersection.
void IntersectionHelper::step_single() {
    uint32_t next = prev_ + run_read(run_it_);
    if (next < *seq_it_) {
        prev_ = next;
        run_it_ = run_forward(run_it_);
        return;
    }
    if (*seq_it_ == next) {
        *seq_out_++ = *seq_it_;
        prev_ = next;
        run_it_ = run_forward(run_it_);
    }
    seq_it_++;
}

// Read 8 bytes under run_it_. If all are small, handle them all.
bool IntersectionHelper::step_by_8() {
    constexpr int BATCH_SIZE = 8;
    constexpr uint64_t VLE_MASK = 0x8080808080808080UL;

    uint64_t *as_qword = (uint64_t *)run_it_;
    uint64_t hit = (*as_qword & VLE_MASK);
    if (hit != 0) {
        return false;
    }

    uint32_t after_batch = prev_ + BATCH_SIZE;
    after_batch += std::accumulate(run_it_, run_it_ + BATCH_SIZE, 0);

    if (after_batch < *seq_it_) {
        run_it_ += BATCH_SIZE;
        prev_ = after_batch;
        return true;
    }

    for (uint8_t *end = run_it_ + BATCH_SIZE; run_it_ < end && seq_it_ < seq_end_;) {
        uint32_t next = prev_ + *run_it_ + 1;
        if (next < *seq_it_) {
            prev_ = next;
            run_it_ += 1;
            continue;
        }
        if (*seq_it_ == next) {
            *seq_out_++ = *seq_it_;
            prev_ = next;
            run_it_ += 1;
        }
        seq_it_++;
    }
    return true;
}

void IntersectionHelper::intersect_by_8() {
    while (run_it_ < run_end_ - 8 && seq_it_ < seq_end_) {
        if (step_by_8()) {
            continue;
        }
        step_single();
    }
}

void IntersectionHelper::intersect() {
    intersect_by_8();
    while (run_it_ < run_end_ && seq_it_ < seq_end_) {
        step_single();
    }
}

void SortedRun::do_and(SortedRun &other) {
    decompress();
    std::vector<uint32_t>::iterator new_end;
    if (other.is_compressed()) {
        IntersectionHelper helper(&sequence_, &other.run_);
        helper.intersect();
        new_end = begin() + helper.result_size();
    } else {
        new_end = std::set_intersection(
            other.begin(), other.end(), begin(), end(), begin());
    }
    sequence_.erase(new_end, end());
}

void SortedRun::decompress() {
    if (run_.empty()) {
        // Already decompressed
        return;
    }

    sequence_ = read_compressed_run(run_.data(), run_.data() + run_.size());
    std::vector<uint8_t> empty;
    run_.swap(empty);
}

SortedRun SortedRun::pick_common(int cutoff,
                                 std::vector<SortedRun *> &sources) {
    // returns all FileIds which appear at least `cutoff` times among provided
    // `sources`
    using FileIdRange = std::pair<std::vector<FileId>::const_iterator,
                                  std::vector<FileId>::const_iterator>;
    std::vector<FileId> result;
    std::vector<FileIdRange> heads;
    heads.reserve(sources.size());

    for (auto source : sources) {
        source->decompress();
        if (!source->empty()) {
            heads.emplace_back(std::make_pair(source->begin(), source->end()));
        }
    }

    while (static_cast<int>(heads.size()) >= cutoff) {
        // pick lowest possible FileId value among all current heads
        int min_index = 0;
        FileId min_id = *heads[0].first;
        for (int i = 1; i < static_cast<int>(heads.size()); i++) {
            if (*heads[i].first < min_id) {
                min_index = i;
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

    return SortedRun(std::move(result));
}

const std::vector<uint32_t> &SortedRun::decompressed() {
    decompress();
    return sequence_;
}
