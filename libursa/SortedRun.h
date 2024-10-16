#include "Core.h"
#include <emmintrin.h>

uint32_t run_read(uint8_t *pos);
uint8_t *run_forward(uint8_t *pos);

class IntersectionHelper {
    uint8_t *run_it_;
    uint8_t *run_end_;
    int32_t prev_;
    uint32_t *seq_start_;
    uint32_t *seq_it_;
    uint32_t *seq_end_;
    uint32_t *seq_out_;

    bool step_by_8();
    void step_single();
    void intersect_by_8();

   public:
    IntersectionHelper(std::vector<uint32_t> *seq, std::vector<uint8_t> *run)
    :run_it_(run->data()), run_end_(run->data() + run->size()), prev_(-1), seq_start_(seq->data()), seq_it_(seq->data()), seq_end_(seq->data() + seq->size()), seq_out_(seq->data()) {}

    size_t result_size() const { return seq_out_ - seq_start_; }
    void intersect();
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

    // Checks if the current run is empty.
    bool empty() const { return sequence_.empty() && run_.empty(); }

    // Does the OR operation with the other vector, overwrites this object.
    void do_or(SortedRun &other);

    // Does the AND operation with the other vector, overwrites this object.
    void do_and(SortedRun &other);

    // Does the MIN_OF operation on specified operands. Allocates a new reuslt.
    static SortedRun pick_common(int cutoff, std::vector<SortedRun *> &sources);

    // When you really need to clone the run - TODO remove.
    SortedRun clone() const { return *this; }

    // Returns a reference to a vector with FileIds of the current run.
    const std::vector<uint32_t> &decompressed();
};
