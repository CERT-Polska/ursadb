#include "SortedRun.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

#include "Utils.h"

uint32_t RunIterator::current() const {
    uint64_t acc = 0;
    uint32_t shift = 0;
    for (uint8_t *it = pos_;; it++) {
        uint32_t next = *it;
        acc += (next & 0x7FU) << shift;
        shift += 7U;
        if ((next & 0x80U) == 0) {
            return prev_ + acc + 1;
        }
    }
}

uint8_t *RunIterator::nextpos() {
    for (uint8_t *it = pos_;; it++) {
        if ((*it & 0x80) == 0) {
            return it + 1;
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

RunIterator SortedRun::comp_begin() {
    validate_compression(true);
    return RunIterator(run_.data());
}

RunIterator SortedRun::comp_end() {
    validate_compression(true);
    return RunIterator(run_.data() + run_.size());
}

void SortedRun::do_or(SortedRun &other) {
    // In almost every case this is already decompressed.
    decompress();
    std::vector<FileId> new_results;
    if (other.is_compressed()) {
        // Unlikely case, in most cases both runs are already decompressed.
        std::set_union(other.comp_begin(), other.comp_end(), begin(), end(),
                       std::back_inserter(new_results));
    } else {
        std::set_union(other.begin(), other.end(), begin(), end(),
                       std::back_inserter(new_results));
    }
    std::swap(new_results, sequence_);
}

// Performance critical method. As of the current version, in some tests,
// more than half of the time is spent ANDing decompressed and compressed runs.
// We expect the compressed set to be large, and sequence to be short.
//
// About the fast case: integers on disk are Variable Length Encoded (VLE).
// This means that interes <= 0x7f are encoded as a single byte, and larger
// ones use multiple bytes (with 0x80 bit used as a continuation bit).
// This function optimizes the common case where most VLE integers are
// small, i.e. are stored as a single byte without additional encoding.
void RunIterator::do_and(RunIterator begin, RunIterator end,
                         std::vector<uint32_t> *target) {
    std::vector<uint32_t> &sequence = *target;
    int out_ndx = 0;
    int ndx = 0;
    int size = sequence.size();

    RunIterator it = begin;
    while (it.pos_ < end.pos_ && ndx < size) {
        // Handle the fast-case. This is purely an optimization, the function
        // will still function properly (but slower) with this `if` removed.
        // Fast case: the next 8 VLE bytes are all small (0x80 bit not set).
        constexpr int BATCH_SIZE = 8;
        constexpr uint64_t VLE_MASK = 0x8080808080808080UL;
        uint64_t *as_qword = (uint64_t *)it.pos_;
        if (it.pos_ + BATCH_SIZE < end.pos_ && (*as_qword & VLE_MASK) == 0) {
            // Fast case of the fast case - if the pointer after processing the
            // current 8 bytes is still smaller than the next element of
            // sequence, just get the sum and skip 8 elements forward.
            uint32_t after_batch = it.prev_ + BATCH_SIZE;
            after_batch += std::accumulate(it.pos_, it.pos_ + BATCH_SIZE, 0);
            if (after_batch < sequence[ndx]) {
                it.forward(BATCH_SIZE, after_batch);
                continue;
            }

            // Regular fast case - do the intersection without decoding VLE
            // integers (remember, we know they are all small, i.e. one byte).
            // Basically the same logic as regular case (see below).
            for (uint8_t *end = it.pos_ + 8; it.pos_ < end && ndx < size;) {
                uint32_t next = it.prev_ + *it.pos_ + 1;
                if (next < sequence[ndx]) {
                    it.forward(1, next);
                    continue;
                }
                if (sequence[ndx] == next) {
                    sequence[out_ndx++] = sequence[ndx];
                    it.forward(1, next);
                }
                ndx += 1;
            }
            continue;
        }

        // Regular set intersection logic (non-optimized/default case).
        // This is basically equivalent to std::set_intersection.
        uint32_t next = *it;
        if (next < sequence[ndx]) {
            ++it;
            continue;
        }
        if (sequence[ndx] == next) {
            sequence[out_ndx++] = sequence[ndx];
            ++it;
        }
        ndx += 1;
    }

    // Clean up elements from the end of the ANDed vector.
    sequence.erase(sequence.begin() + out_ndx, sequence.end());
}

void SortedRun::do_and(SortedRun &other) {
    // Benchmarking shows that handling a situation where this->is_compressed()
    // makes the code *slower*. I assume that's because of memory efficiency.
    decompress();
    if (other.is_compressed()) {
        RunIterator::do_and(other.comp_begin(), other.comp_end(), &sequence_);
    } else {
        std::vector<uint32_t>::iterator new_end = std::set_intersection(
            other.begin(), other.end(), begin(), end(), begin());
        sequence_.erase(new_end, sequence_.end());
    }
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
