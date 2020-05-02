#include "OnDiskIndex.h"

#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>

#include "FeatureFlags.h"
#include "Query.h"
#include "QueryGraph.h"
#include "Utils.h"
#include "spdlog/spdlog.h"

#pragma pack(1)
struct OnDiskIndexHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t raw_type;
    uint32_t reserved;
};
#pragma pack()

constexpr int DATA_OFFSET = 16;
constexpr uint64_t RUN_ARRAY_SIZE = (NUM_TRIGRAMS + 1) * sizeof(uint64_t);

OnDiskIndex::OnDiskIndex(const std::string &fname)
    : fname(fs::path(fname).filename()),
      fpath(fs::path(fname)),
      ndxfile(fname) {
    OnDiskIndexHeader header;

    index_size = ndxfile.size();

    if (index_size < DATA_OFFSET + RUN_ARRAY_SIZE) {
        throw std::runtime_error("corrupted index, file is too small");
    }

    ndxfile.pread(&header, sizeof(OnDiskIndexHeader), 0);

    if (header.magic != DB_MAGIC) {
        throw std::runtime_error("invalid magic, not a catdata");
    }

    if (header.version != OnDiskIndex::VERSION) {
        throw std::runtime_error("unsupported version");
    }

    if (!is_valid_index_type(header.raw_type)) {
        throw std::runtime_error("invalid index type");
    }

    if (header.reserved != 0) {
        throw std::runtime_error("reserved field != 0");
    }

    ntype = static_cast<IndexType>(header.raw_type);
}

// Returns all files that can be matched to given graph.
QueryResult OnDiskIndex::query(const QueryGraph &graph) const {
    spdlog::debug("Graph query for {}", get_index_type_name(index_type()));

    QueryFunc oracle = [this](uint64_t raw_gram) {
        auto gram = convert_gram(index_type(), raw_gram);
        if (gram) {
            return QueryResult(std::move(query_primitive(*gram)));
        }
        return QueryResult::everything();
    };
    return graph.run(oracle);
}

std::pair<uint64_t, uint64_t> OnDiskIndex::get_run_offsets(
    TriGram trigram) const {
    uint64_t ptrs[2];
    uint64_t offset = index_size - RUN_ARRAY_SIZE + trigram * sizeof(uint64_t);
    ndxfile.pread(ptrs, sizeof(ptrs), offset);
    return std::make_pair(ptrs[0], ptrs[1]);
}

std::vector<FileId> OnDiskIndex::get_run(uint64_t ptr,
                                         uint64_t next_ptr) const {
    uint64_t run_length = next_ptr - ptr;

    if (ptr > next_ptr || next_ptr > index_size) {
        // TODO() - Which index? Which run?
        throw std::runtime_error(
            "internal error: index is corrupted, invalid run");
    }

    std::vector<uint8_t> run_bytes(run_length);
    ndxfile.pread(run_bytes.data(), run_length, ptr);
    return read_compressed_run(run_bytes.data(),
                               run_bytes.data() + run_bytes.size());
}

std::vector<FileId> OnDiskIndex::query_primitive(TriGram trigram) const {
    std::pair<uint64_t, uint64_t> offsets = get_run_offsets(trigram);
    return get_run(offsets.first, offsets.second);
}

unsigned long OnDiskIndex::real_size() const { return fs::file_size(fpath); }

// Finds the biggest batch size starting from `trigram`, for which size of all
// runs is still smaller than max_bytes.
uint64_t find_max_batch(const std::vector<IndexMergeHelper> &indexes,
                        uint32_t trigram, uint64_t max_bytes) {
    for (int i = 1; trigram + i < NUM_TRIGRAMS; i++) {
        uint64_t batch_bytes = 0;
        for (const auto &ndx : indexes) {
            batch_bytes += ndx.run(trigram, i).size();
        }
        if (batch_bytes > max_bytes) {
            return i - 1;
        }
    }
    return NUM_TRIGRAMS - trigram;
}

// Merge the indexes, and stream the results to the `out` stream immediately.
// This function tries to batch reads, which makes it much more efficient on
// HDDs (on SSDs the difference is not noticeable).
// Instead of reading one ngram run at a time, read up to MAX_BATCH ngrams.
// In a special case of batch size=1, this is equivalent to the older method.
void OnDiskIndex::on_disk_merge_core(
    const std::vector<IndexMergeHelper> &indexes, RawFile *out,
    TaskSpec *task) {
    // Offsets to every run in the file (including the header).
    std::vector<uint64_t> offsets(NUM_TRIGRAMS + 1);

    // Current offset in the file (equal to size of the header intially).
    uint64_t out_offset = 16;

    // Arbitrary number describing how much RAM we want to spend on the run
    // cache during the batched stream pass.
    constexpr uint64_t MAX_BATCH_BYTES = 2ULL * 1024ULL * 1024ULL * 1024ULL;

    // Vector used for all merge passes (to avoid unnecessary reallocations).
    std::vector<uint8_t> batch_vector(MAX_BATCH_BYTES);
    uint8_t *batch_data = batch_vector.data();

    PosixRunWriter writer(out->get());

    // Main merge loop.
    TriGram trigram = 0;
    while (trigram < NUM_TRIGRAMS) {
        uint64_t batch_size = find_max_batch(indexes, trigram, MAX_BATCH_BYTES);

        if (batch_size == 0) {
            // TODO fallback to old unbatched merge method.
            spdlog::error("Merge too big, can't fit into MAX_BATCH_BYTES");
            throw std::runtime_error("Can't merge, batch size too big");
        }

        // Read batch_size runs at once.
        uint8_t *batch_ptr = batch_data;
        for (const auto &ndx : indexes) {
            OnDiskRun run = ndx.run(trigram, batch_size);
            ndx.index->ndxfile.pread(batch_ptr, run.size(), run.start());
            batch_ptr += run.size();
        }

        // Write the runs to the output file in a proper order.
        for (int i = 0; i < static_cast<int>(batch_size); i++) {
            offsets[trigram + i] = out_offset;
            uint64_t base_bytes = 0;
            FileId base_files = 0;
            for (const auto &ndx : indexes) {
                uint8_t *run_base = batch_data + base_bytes;
                uint8_t *run_start = run_base + ndx.run(trigram, i).size();
                uint8_t *run_end = run_base + ndx.run(trigram, i + 1).size();
                writer.write_raw(base_files, run_start, run_end);
                base_bytes += ndx.run(trigram, batch_size).size();
                base_files += ndx.file_count;
            }
            out_offset += writer.bytes_written();
            writer.reset();
        }

        // Bookkeeping - update progress and current trigram ndx.
        if (task != nullptr) {
            task->add_progress(batch_size);
        }
        trigram += batch_size;
    }
    writer.flush();

    // Write the footer - 128MB header with run offsets.
    offsets[NUM_TRIGRAMS] = out_offset;
    out->write<uint64_t>(offsets.data(), offsets.size());
}

// Creates necessary headers, and forwards the real work to merge_core
void OnDiskIndex::on_disk_merge(const fs::path &db_base,
                                const std::string &fname, IndexType merge_type,
                                const std::vector<IndexMergeHelper> &indexes,
                                TaskSpec *task) {
    RawFile out(db_base / fname, O_WRONLY | O_CREAT | O_EXCL, 0600);

    if (!std::all_of(indexes.begin(), indexes.end(),
                     [merge_type](const IndexMergeHelper &ndx) {
                         return ndx.index->ntype == merge_type;
                     })) {
        throw std::runtime_error("Unexpected index type during merge");
    }

    OnDiskIndexHeader header;
    header.magic = DB_MAGIC;
    header.version = OnDiskIndex::VERSION;
    header.raw_type = (uint32_t)merge_type;
    header.reserved = 0;

    out.write(reinterpret_cast<uint8_t *>(&header), sizeof(header));

    on_disk_merge_core(indexes, &out, task);
}

std::vector<uint64_t> OnDiskIndex::read_run_offsets() const {
    std::vector<uint64_t> run_offsets(NUM_TRIGRAMS + 1);
    ndxfile.pread(run_offsets.data(), RUN_ARRAY_SIZE,
                  index_size - RUN_ARRAY_SIZE);
    return run_offsets;
}
