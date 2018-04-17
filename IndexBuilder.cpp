#include "IndexBuilder.h"

#include <algorithm>
#include <iostream>

IndexBuilder::IndexBuilder(IndexType ntype) : raw_index(NUM_TRIGRAMS), ntype(ntype) {}

void IndexBuilder::add_trigram(FileId fid, TriGram val) {
    if (raw_index[val].empty() || raw_index[val].back() != fid) {
        // guard against indexing same (file, trigram) pair twice
        raw_index[val].push_back(fid);
    }
}

void IndexBuilder::save(const std::string &fname) {
    std::ofstream out(fname, std::ofstream::binary | std::ofstream::out);

    uint32_t magic = DB_MAGIC;
    uint32_t version = 5;
    uint32_t ndx_type = static_cast<uint32_t>(ntype);
    uint32_t reserved = 0;

    out.write((char *)&magic, 4);
    out.write((char *)&version, 4);
    out.write((char *)&ndx_type, 4);
    out.write((char *)&reserved, 4);

    std::vector<uint32_t> offsets(NUM_TRIGRAMS + 1);

    for (int i = 0; i < NUM_TRIGRAMS; i++) {
        offsets[i] = (uint32_t)out.tellp();
        std::sort(raw_index[i].begin(), raw_index[i].end());
        raw_index[i].erase(unique(raw_index[i].begin(), raw_index[i].end()), raw_index[i].end());
        compress_run(raw_index[i], out);
    }
    offsets[NUM_TRIGRAMS] = (uint32_t)out.tellp();

    out.write((char *)offsets.data(), (NUM_TRIGRAMS + 1) * 4);
    out.close();
}

void IndexBuilder::add_file(FileId fid, const uint8_t *data, size_t size) {
    TrigramGenerator generator = get_generator_for(ntype);
    std::vector<TriGram> out = generator(data, size);

    for (TriGram gram3 : out) {
        add_trigram(fid, gram3);
    }
}
