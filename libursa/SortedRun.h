#include <iterator>

#include "Core.h"
#include "Utils.h"

class SortedRunIterator
    : public std::iterator<std::input_iterator_tag, uint32_t> {
    uint8_t *ptr_;

   public:
    SortedRunIterator(uint8_t *ptr) : ptr_(ptr) {}

    uint32_t operator*() const { return 0; }

    uint32_t operator++() { return *(*this); }

    uint32_t operator++(int) {
        uint32_t result = *(*this);
        ++(*this);
        return result;
    }

    bool operator==(const SortedRunIterator &other) {
        return ptr_ == other.ptr_;
    }

    bool operator!=(const SortedRunIterator &other) {
        return !(*this == other);
    }
};

std::vector<uint8_t> compress_sequence(const std::vector<uint32_t> &) {
    return {};
}

class SortedRun {
    std::vector<uint8_t> sequence_;

   public:
    SortedRun() : sequence_{} {}
    explicit SortedRun(std::vector<uint32_t> &&sequence)
        : sequence_(compress_sequence(sequence)) {}
    explicit SortedRun(std::vector<uint8_t> &&sequence)
        : sequence_(std::move(sequence)) {}

    bool empty() const { return sequence_.empty(); }

    bool operator==(const SortedRun &other) const {
        return sequence_ == other.sequence_;
    }

    void do_or(const SortedRun &other);
    void do_and(const SortedRun &other);

    static SortedRun pick_common(int cutoff,
                                 const std::vector<const SortedRun *> &sources);

    // When you really need to clone the run - TODO remove.
    SortedRun clone() const { return *this; }

    SortedRunIterator begin() const { return this->begin(); }

    SortedRunIterator cbegin() const { return this->begin(); }

    SortedRunIterator begin() { return SortedRunIterator(sequence_.data()); }

    SortedRunIterator end() const { return this->end(); }

    SortedRunIterator cend() const { return this->end(); }

    SortedRunIterator end() {
        return SortedRunIterator(sequence_.data() + sequence_.size());
    }
};
