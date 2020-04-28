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

bool OnDiskIndex::internal_expand(QString::const_iterator qit, uint8_t *out,
                                  size_t pos, size_t comb_len,
                                  const TrigramGenerator &gen,
                                  QueryResult &res) const {
    if (pos >= comb_len) {
        bool was_generated = false;

        gen(out, comb_len, [&](TriGram val) {
            was_generated = true;
            res.do_or(QueryResult(query_primitive(val)));
        });

        return was_generated;
    }

    auto j = (uint8_t)qit->val();

    do {
        out[pos] = j;
        bool was_generated =
            internal_expand(qit + 1, out, pos + 1, comb_len, gen, res);

        if (!was_generated) {
            // sequence was not recognized by given index
            // this may happen for indexes like text4 or wide8
            return false;
        }

        switch (qit->type()) {
            case QTokenType::CHAR:
                return true;
            case QTokenType::WILDCARD:
                if (j == 0xFF) {
                    return true;
                } else {
                    ++j;
                }
                break;
            case QTokenType::LWILDCARD:
                if (j == (qit->val() | 0xFU)) {
                    return true;
                } else {
                    ++j;
                }
                break;
            case QTokenType::HWILDCARD:
                if (j == (qit->val() | 0xF0U)) {
                    return true;
                } else {
                    j += 0x10;
                }
                break;
            default:
                throw std::runtime_error("unknown token type");
        }
    } while (true);
}

QueryResult OnDiskIndex::expand_wildcards(const QString &qstr, size_t len,
                                          const TrigramGenerator &gen) const {
    uint8_t out[len];
    QueryResult total = QueryResult::everything();

    if (qstr.size() < len) {
        return total;
    }

    for (unsigned int i = 0; i <= qstr.size() - len; i++) {
        QueryResult res = QueryResult::empty();
        bool success =
            internal_expand(qstr.cbegin() + i, out, 0, len, gen, res);
        if (success) {
            total.do_and(std::move(res));
        }
    }

    return total;
}

// Expands the query to a query graph, but at the same time is careful not to
// generate a query graph that is too big.
// This is a pretty limiting heuristics and it can't account for more complex
// tokens (for example, `(11 | 22 33)`). The proper solution is much harder and
// will need a efficient graph pruning algorithm. Nevertheless, this solution
// works and can support everything that the current parser can.
QueryGraph to_query_graph(const QString &str, int ngram_size) {
    // Graph representing the final query equivalent to qstring.
    QueryGraph result;

    // Maximum number of possible values for the edge to be considered.
    // If token has more than MAX_EDGE possible values, it will never start
    // or end a subgraph. This is to avoid starting a subquery with `??`.
    constexpr uint32_t MAX_EDGE = 16;

    // Maximum number of possible values for ngram to be considered. If ngram
    // has more than MAX_NGRAM possible values, it won't be included in the
    // graph and the graph will be split into one or more subgraphs.
    constexpr uint32_t MAX_NGRAM = 256 * 256;

    spdlog::info("Expand+prune for a query graph size={}", ngram_size);

    int offset = 0;
    while (offset < str.size()) {
        spdlog::info("Looking for a new start edge");
        // Check if this node can become an edge.
        if (str[offset].num_possible_values() > MAX_EDGE) {
            offset++;
            continue;
        }

        // Insert first ngram_size - 1 tokens.
        std::vector<QToken> tokens;
        for (int i = 0; i < ngram_size - 1 && offset < str.size(); i++) {
            tokens.emplace_back(str[offset].clone());
            offset++;
        }

        // Now insert tokens ending ngram, as long as it is small enough.
        while (offset < str.size()) {
            uint64_t num_possible = 1;
            for (int i = 0; i < ngram_size; i++) {
                num_possible *= str[offset - i].num_possible_values();
            }
            if (num_possible > MAX_NGRAM) {
                break;
            }
            tokens.emplace_back(str[offset].clone());
            offset++;
        }

        // Finally, prune the subquery from the right (using MAX_EDGE).
        while (tokens.back().num_possible_values() > MAX_EDGE) {
            // This is safe, because first element is < MAX_EDGE.
            tokens.pop_back();
        }

        if (tokens.size() < ngram_size) {
            continue;
        }

        spdlog::info("Got a subgraph candidate, size={}", tokens.size());
        QueryGraph subgraph{QueryGraph::from_qstring(tokens)};

        for (int i = 0; i < ngram_size - 1; i++) {
            spdlog::info("Computing dual graph ({} nodes)", subgraph.size());
            subgraph = std::move(subgraph.dual());
        }

        spdlog::info("Merging subgraph into the result");
        result.join(std::move(subgraph));
    }

    spdlog::info("Query graph expansion succeeded ({} nodes)", result.size());
    return result;
}

QueryResult OnDiskIndex::query_str(const QString &str) const {
    TrigramGenerator generator = get_generator_for(ntype);

    size_t input_len = 0;

    switch (index_type()) {
        case IndexType::GRAM3:
            input_len = 3;
            break;
        case IndexType::HASH4:
            input_len = 4;
            break;
        case IndexType::TEXT4:
            input_len = 4;
            break;
        case IndexType::WIDE8:
            input_len = 8;
            break;
        default:
            throw std::runtime_error("unhandled index type");
    }

    if (::feature::query_graphs && input_len <= 4) {
        spdlog::info("Experimental graph query for {}",
                     get_index_type_name(index_type()));

        QueryFunc oracle = [this](uint32_t raw_gram) {
            auto gram = convert_gram(index_type(), raw_gram);
            if (gram) {
                return QueryResult(std::move(query_primitive(*gram)));
            }
            return QueryResult::everything();
        };
        QueryGraph graph{to_query_graph(str, input_len)};
        return graph.run(oracle);
    }
    return expand_wildcards(str, input_len, generator);
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
    1;
    for (int i = 2; trigram + i < NUM_TRIGRAMS; i++) {
        uint64_t batch_bytes = 0;
        for (const auto &ndx : indexes) {
            batch_bytes += ndx.run(trigram, i).size();
        }
        if (batch_bytes > max_bytes) {
            return i;
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
    const std::vector<IndexMergeHelper> &indexes, RawFile *out, Task *task) {
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

        // Read batch_size runs at once.
        uint8_t *batch_ptr = batch_data;
        for (const auto &ndx : indexes) {
            OnDiskRun run = ndx.run(trigram, batch_size);
            ndx.index->ndxfile.pread(batch_ptr, run.size(), run.start());
            batch_ptr += run.size();
        }

        // Write the runs to the output file in a proper order.
        for (int i = 0; i < batch_size; i++) {
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
            task->work_done += batch_size;
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
                                Task *task) {
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
