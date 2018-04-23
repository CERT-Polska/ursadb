#include "OnDiskIndex.h"

#include <algorithm>
#include <iostream>

#include "Query.h"
#include "Utils.h"

OnDiskIndex::OnDiskIndex(const std::string &fname) : disk_map(fname) {
    constexpr uint64_t RUN_OFFSET_ARRAY_SIZE = (NUM_TRIGRAMS + 1) * sizeof(uint64_t);
    if (disk_map.size() < 16 + RUN_OFFSET_ARRAY_SIZE) {
        throw std::runtime_error("corrupted index, file is too small");
    }

    const uint8_t *data = disk_map.data();
    uint32_t magic = *(uint32_t *)&data[0];
    uint32_t version = *(uint32_t *)&data[4];
    uint32_t raw_type = *(uint32_t *)&data[8];
    uint32_t reserved = *(uint32_t *)&data[12];

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

QueryResult OnDiskIndex::query_str(const std::string &str) const {
    TrigramGenerator generator = get_generator_for(ntype);
    auto trigrams = generator((uint8_t *)str.data(), str.size());
    QueryResult result = QueryResult::everything();

    for (auto trigram : trigrams) {
        result.do_and(query_primitive(trigram));
    }

    return result;
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
        std::string fname, IndexType merge_type, const std::vector<IndexMergeHelper> &indexes) {
    std::ofstream out(fname, std::ofstream::binary | std::ofstream::out);

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

    for (int trigram = 0; trigram < NUM_TRIGRAMS; trigram++) {
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
    }
    out_offsets[NUM_TRIGRAMS] = (uint64_t)out.tellp();

    out.write((char *)out_offsets.data(), (NUM_TRIGRAMS + 1) * sizeof(uint64_t));
    out.close();
}
