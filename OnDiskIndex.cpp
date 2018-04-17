#include "OnDiskIndex.h"

#include <algorithm>
#include <iostream>

#include "Query.h"
#include "Utils.h"

OnDiskIndex::OnDiskIndex(const std::string &fname) : disk_map(fname) {
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
    run_offsets = (uint32_t *)&data[disk_map.size() - (NUM_TRIGRAMS + 1) * 4];
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
    uint32_t ptr = run_offsets[trigram];
    uint32_t next_ptr = run_offsets[trigram + 1];

    const uint8_t *data = disk_map.data();
    std::vector<FileId> out = read_compressed_run(&data[ptr], &data[next_ptr]);
    return out;
}

void OnDiskIndex::on_disk_merge(std::string fname, IndexType merge_type,
                                const std::vector<IndexMergeHelper> &indexes) {
    std::ofstream out(fname, std::ofstream::binary | std::ofstream::out);

    if (!std::all_of(indexes.begin(), indexes.end(), [merge_type](const IndexMergeHelper &ndx) {
            return ndx.index->ntype == merge_type;
        })) {
        throw new std::runtime_error("Unexpected index type during merge");
    }

    uint32_t magic = DB_MAGIC;
    uint32_t version = OnDiskIndex::VERSION;
    uint32_t ndx_type = (uint32_t)merge_type;
    uint32_t reserved = 0;

    out.write((char *)&magic, 4);
    out.write((char *)&version, 4);
    out.write((char *)&ndx_type, 4);
    out.write((char *)&reserved, 4);

    std::vector<uint32_t> out_offsets(NUM_TRIGRAMS + 1);
    std::vector<uint32_t> in_offsets(indexes.size());

    for (int i = 0; i < NUM_TRIGRAMS; i++) {
        out_offsets[i] = (uint32_t)out.tellp();
        std::vector<FileId> all_ids;
        FileId baseline = 0;
        for (const IndexMergeHelper &helper : indexes) {
            std::vector<FileId> new_ids =
                read_compressed_run(helper.index->data() + helper.index->run_offsets[i],
                                    helper.index->data() + helper.index->run_offsets[i + 1]);
            for (FileId id : new_ids) {
                all_ids.push_back(id + baseline);
            }
            baseline += helper.file_count;
        }
        compress_run(all_ids, out);
    }
    out_offsets[NUM_TRIGRAMS] = (uint32_t)out.tellp();

    out.write((char *)out_offsets.data(), (NUM_TRIGRAMS + 1) * 4);
    out.close();
}
