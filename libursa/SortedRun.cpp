#include "SortedRun.h"

#include <algorithm>

void SortedRun::do_or(const SortedRun &other) {
    std::vector<FileId> new_results;
    std::set_union(other.begin(), other.end(), sequence_.begin(),
                   sequence_.end(), std::back_inserter(new_results));
    std::swap(new_results, sequence_);
}

void SortedRun::do_and(const SortedRun &other) {
    auto new_end =
        std::set_intersection(other.begin(), other.end(), sequence_.begin(),
                              sequence_.end(), sequence_.begin());
    sequence_.erase(new_end, sequence_.end());
}

SortedRun SortedRun::pick_common(
    int cutoff, const std::vector<const SortedRun *> &sources) {
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
