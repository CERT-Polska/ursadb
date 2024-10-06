#include "Core.h"

// Iterate over a compressed run representation.
// "Run" here means a sorted list of FileIDs (this name is used in the
// codebase).  And a "compressed" run format is described in the documentation
// "ondiskformat.md", in the "Index" section.
class RunIterator : public std::iterator<std::forward_iterator_tag, uint32_t> {
    typedef RunIterator iterator;
    uint8_t *pos_;
    int32_t prev_;

    uint32_t current() const;
    uint8_t *nextpos();

   public:
    RunIterator(uint8_t *run) : pos_(run), prev_(-1) { }
    ~RunIterator() {}

    RunIterator &operator++() {
        prev_ = current();
        pos_ = nextpos();
        return *this;
    }

    uint32_t operator*() const { return current(); }
    bool operator!=(const iterator &rhs) const { return pos_ != rhs.pos_; }
};

// This class represents a "run" - a sorted list of FileIDs. This can be
// a list of files matching a given ngram, for example. "Sorted" here is
// redundant - there are no unsorted runs.
// There are two possible representations, "compressed" and "uncompressed".
// In the "compressed" representation we store raw data in vector<uint_8>
// and parse it on the go (see the file format documentation). The
// "uncompressed" representation is a raw (sorted) list of FileIDs. Compressed
// representation is useful to avoid heavy allocations for every file read.
class SortedRun {
    // Only one of the following vector is nonempty at any time.
    std::vector<uint32_t> sequence_;  // Uncompressed representation
    std::vector<uint8_t> run_;        // Compressed representation

    // Assert that the run is compressed/uncompressed, throw otherwise.
    void validate_compression(bool expected);

    // Returns true if this run is compressed and non-empty.
    bool is_compressed() const { return !run_.empty(); }

    // Force decompression of the compressed representation (noop otherwise).
    void decompress();

    // Iterate over the decompressed representation (throws if compressed)
    std::vector<uint32_t>::iterator begin();
    std::vector<uint32_t>::iterator end();

    // Iterate over the compressed representation (throws if decompressed)
    RunIterator comp_begin();
    RunIterator comp_end();

    SortedRun(const SortedRun &other) = default;

   public:
    SortedRun() : sequence_{}, run_{} {}

    // Create a new run (with a decompressed representation)
    explicit SortedRun(std::vector<uint32_t> &&sequence)
        : sequence_{std::move(sequence)} {}

    // Create a new run (with a compressed representation)
    explicit SortedRun(std::vector<uint8_t> &&run) : run_{std::move(run)} {}

    SortedRun(SortedRun &&other)
        : sequence_(other.sequence_), run_(other.run_) {}
    SortedRun &operator=(SortedRun &&) = default;

    bool empty() const { return sequence_.empty() && run_.empty(); }

    void do_or(SortedRun &other);
    void do_and(SortedRun &other);

    static SortedRun pick_common(int cutoff, std::vector<SortedRun *> &sources);

    // When you really need to clone the run - TODO remove.
    SortedRun clone() const { return *this; }

    // Returns a reference to a vector with FileIds of the current run.
    const std::vector<uint32_t> &decompressed();
};
