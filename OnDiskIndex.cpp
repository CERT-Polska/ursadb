#include "OnDiskIndex.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

#include "Query.h"
#include "Utils.h"

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
        : fname(fs::path(fname).filename())
        , fpath(fs::path(fname))
        , ndxfile(fname) {
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

bool OnDiskIndex::internal_expand(QString::const_iterator qit, uint8_t *out, size_t pos, size_t comb_len,
                                  const TrigramGenerator &gen, QueryResult &res) const {
    if (pos >= comb_len) {
        bool was_generated = false;

        gen(out, comb_len, [&] (TriGram val) {
            was_generated = true;
            res.do_or(query_primitive(val));
        });

        return was_generated;
    }

    auto j = (uint8_t)qit->val();

    do {
        out[pos] = j;
        bool was_generated = internal_expand(qit + 1, out, pos + 1, comb_len, gen, res);

        if (!was_generated) {
            // sequence was not recognized by given index
            // this may happen for indexes like text4 or wide8
            return false;
        }

        switch (qit->type()) {
            case QTokenType::CHAR:
                return true;
            case QTokenType::WILDCARD:
                if (j == 0xFF) { return true; } else { ++j; } break;
            case QTokenType::LWILDCARD:
                if (j == (qit->val() | 0xFU)) { return true; } else { ++j; } break;
            case QTokenType::HWILDCARD:
                if (j == (qit->val() | 0xF0U)) { return true; } else { j += 0x10; } break;
        }
    } while (true);
}

QueryResult OnDiskIndex::expand_wildcards(const QString &qstr, size_t len, const TrigramGenerator &gen) const {
    uint8_t out[len];
    QueryResult total = QueryResult::everything();

    if (qstr.size() < len) {
        return total;
    }

    for (unsigned int i = 0; i <= qstr.size() - len; i++) {
        QueryResult res = QueryResult::empty();
        bool success = internal_expand(qstr.cbegin() + i, out, 0, len, gen, res);
        if (success) {
            total.do_and(std::move(res));
        }
    }

    return total;
}

QueryResult OnDiskIndex::query_str(const QString &str) const {
    TrigramGenerator generator = get_generator_for(ntype);

    size_t input_len = 0;

    switch (index_type()) {
        case IndexType::GRAM3: input_len = 3; break;
        case IndexType::HASH4: input_len = 4; break;
        case IndexType::TEXT4: input_len = 4; break;
        case IndexType::WIDE8: input_len = 8; break;
    }
    if (input_len == 0) {
        throw std::runtime_error("unhandled index type");
    }

    return expand_wildcards(str, input_len, generator);
}

std::pair<uint64_t, uint64_t>  OnDiskIndex::get_run_offsets(TriGram trigram) const {
    uint64_t ptrs[2];
    uint64_t offset = index_size - RUN_ARRAY_SIZE + trigram * sizeof(uint64_t);
    ndxfile.pread(ptrs, sizeof(ptrs), offset);
    return std::make_pair(ptrs[0], ptrs[1]);
}

std::vector<FileId> OnDiskIndex::get_run(uint64_t ptr, uint64_t next_ptr) const {
    uint64_t run_length = next_ptr - ptr;

    if (ptr > next_ptr || next_ptr > index_size) {
        // TODO() - Which index? Which run?
        throw std::runtime_error("internal error: index is corrupted, invalid run");
    }

    std::vector<uint8_t> run_bytes(run_length);
    ndxfile.pread(run_bytes.data(), run_length, ptr);
    return read_compressed_run(
        run_bytes.data(),
        run_bytes.data() + run_bytes.size()
    );
}

std::vector<FileId> OnDiskIndex::query_primitive(TriGram trigram) const {
    std::pair<uint64_t, uint64_t> offsets = get_run_offsets(trigram);
    return get_run(offsets.first, offsets.second);
}

unsigned long OnDiskIndex::real_size() const {
    return fs::file_size(fpath);
}

void OnDiskIndex::on_disk_merge(
        const fs::path &db_base, const std::string &fname, IndexType merge_type,
        const std::vector<IndexMergeHelper> &indexes, Task *task) {
    std::ofstream out;
    out.exceptions(std::ofstream::badbit);
    out.open(db_base / fname, std::ofstream::binary);

    if (!std::all_of(indexes.begin(), indexes.end(), [merge_type](const IndexMergeHelper &ndx) {
        return ndx.index->ntype == merge_type;
    })) {
        throw std::runtime_error("Unexpected index type during merge");
    }

    uint32_t magic = DB_MAGIC;
    uint32_t version = OnDiskIndex::VERSION;
    uint32_t ndx_type = (uint32_t)merge_type;
    uint32_t reserved = 0;

    out.write((char *)&magic, 4);
    out.write((char *)&version, 4);
    out.write((char *)&ndx_type, 4);
    out.write((char *)&reserved, 4);

    std::vector<uint64_t> out_offsets(NUM_TRIGRAMS + 1);

    for (TriGram trigram = 0; trigram < NUM_TRIGRAMS; trigram++) {
        out_offsets[trigram] = (uint64_t)out.tellp();
        FileId baseline = 0;

        RunWriter run_writer(&out);
        for (const IndexMergeHelper &helper : indexes) {
            uint64_t ptr = helper.run_offset_cache[trigram];
            uint64_t next_ptr = helper.run_offset_cache[trigram + 1];
            std::vector<FileId> new_ids = helper.index->get_run(ptr, next_ptr);
            for (FileId id : new_ids) {
                run_writer.write(id + baseline);
            }
            baseline += helper.file_count;
        }

        if (task != nullptr) {
            task->work_done += 1;
        }
    }
    out_offsets[NUM_TRIGRAMS] = (uint64_t)out.tellp();

    out.write((char *)out_offsets.data(), (NUM_TRIGRAMS + 1) * sizeof(uint64_t));
}

std::vector<uint64_t> OnDiskIndex::read_run_offsets() const {
    std::vector<uint64_t> run_offsets(NUM_TRIGRAMS + 1);
    ndxfile.pread(run_offsets.data(), RUN_ARRAY_SIZE, index_size - RUN_ARRAY_SIZE);
    return run_offsets;
}
