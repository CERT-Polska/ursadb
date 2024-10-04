#include "SortedRun.h"

#include <algorithm>
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
            return curr_ + acc + 1;
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

void SortedRun::do_and(SortedRun &other) {
    // Benchmarking shows that handling a situation where this->is_compressed()
    // makes the code *slower*. I assume that's because of memory efficiency.
    decompress();
    std::vector<uint32_t>::iterator new_end;
    if (other.is_compressed()) {
        new_end = std::set_intersection(other.comp_begin(), other.comp_end(),
                                        begin(), end(), begin());
    } else {
        new_end = std::set_intersection(other.begin(), other.end(), begin(),
                                        end(), begin());
    }
    sequence_.erase(new_end, sequence_.end());
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
