#include "OnDiskIndex.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

#include "Query.h"
#include "Utils.h"

OnDiskIndex::OnDiskIndex(const std::string &fname) : disk_map(fname), fname(fs::path(fname).filename()) {
    constexpr uint64_t RUN_OFFSET_ARRAY_SIZE = (NUM_TRIGRAMS + 1) * sizeof(uint64_t);
    if (disk_map.size() < 16 + RUN_OFFSET_ARRAY_SIZE) {
        throw std::runtime_error("corrupted index, file is too small");
    }

    const uint8_t *data = disk_map.data();
    uint32_t magic, version, raw_type, reserved;
    memcpy(&magic,    data +  0, sizeof(uint32_t));
    memcpy(&version,  data +  4, sizeof(uint32_t));
    memcpy(&raw_type, data +  8, sizeof(uint32_t));
    memcpy(&reserved, data + 12, sizeof(uint32_t));

    if (magic != DB_MAGIC) {
        throw std::runtime_error("invalid magic, not a catdata");
    }

    if (version != OnDiskIndex::VERSION) {
        throw std::runtime_error("unsupported version");
    }

    if (!is_valid_index_type(raw_type)) {
        throw std::runtime_error("invalid index type");
    }

    ntype = static_cast<IndexType>(raw_type);
    run_offsets = (uint64_t *)&data[disk_map.size() - RUN_OFFSET_ARRAY_SIZE];
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

std::vector<FileId> OnDiskIndex::query_primitive(TriGram trigram) const {
    uint64_t ptr = run_offsets[trigram];
    uint64_t next_ptr = run_offsets[trigram + 1];
    const uint8_t *data = disk_map.data();

    if (ptr > next_ptr || next_ptr > disk_map.size()) {
        // TODO() - Which index? Which run?
        throw std::runtime_error("internal error: index is corrupted, invalid run");
    }

    return read_compressed_run(&data[ptr], &data[next_ptr]);
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
    std::vector<uint64_t> in_offsets(indexes.size());

    for (TriGram trigram = 0; trigram < NUM_TRIGRAMS; trigram++) {
        out_offsets[trigram] = (uint64_t)out.tellp();
        std::vector<FileId> all_ids;
        FileId baseline = 0;

        for (const IndexMergeHelper &helper : indexes) {
            std::vector<FileId> new_ids = helper.index->query_primitive(trigram);
            for (FileId id : new_ids) {
                all_ids.push_back(id + baseline);
            }
            baseline += helper.file_count;
        }

        compress_run(all_ids, out);

        if (task != nullptr) {
            task->work_done += 1;
        }
    }
    out_offsets[NUM_TRIGRAMS] = (uint64_t)out.tellp();

    out.write((char *)out_offsets.data(), (NUM_TRIGRAMS + 1) * sizeof(uint64_t));
}
