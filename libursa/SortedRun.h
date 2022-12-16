#include "Core.h"

class SortedRun {
    std::vector<uint32_t> sequence_;

   public:
    SortedRun() : sequence_{} {}
    SortedRun(std::vector<uint32_t> &&sequence)
        : sequence_(std::move(sequence)) {}

    bool empty() const { return sequence_.empty(); }

    // Only expose size in bytes, to let the storage implementation change
    // in the future.
    uint64_t size_in_bytes() const {
        return sequence_.size() * sizeof(uint32_t);
    }

    bool operator==(const SortedRun &other) const {
        return sequence_ == other.sequence_;
    }

    void do_or(const SortedRun &other);
    void do_and(const SortedRun &other);

    static SortedRun pick_common(int cutoff,
                                 const std::vector<const SortedRun *> &sources);

    // When you really need to clone the run - TODO remove.
    SortedRun clone() const { return *this; }

    std::vector<uint32_t>::const_iterator begin() const {
        return sequence_.begin();
    }

    std::vector<uint32_t>::const_iterator cbegin() const {
        return sequence_.begin();
    }

    std::vector<uint32_t>::iterator begin() { return sequence_.begin(); }

    std::vector<uint32_t>::const_iterator end() const {
        return sequence_.end();
    }

    std::vector<uint32_t>::const_iterator cend() const {
        return sequence_.end();
    }

    std::vector<uint32_t>::iterator end() { return sequence_.end(); }
};
