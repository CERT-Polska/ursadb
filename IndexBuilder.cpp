#include <iostream>
#include "IndexBuilder.h"


IndexBuilder::IndexBuilder() : raw_index(NUM_TRIGRAMS) {

}

void IndexBuilder::add_trigram(FileId fid, TriGram val) {
    raw_index[val].push_back(fid);
}

void IndexBuilder::save(const std::string &fname) {
    std::ofstream out(fname, std::ofstream::binary);

    uint32_t magic = DB_MAGIC;
    uint32_t version = 5;
    uint32_t ndx_type = 1;
    uint32_t reserved = 0;

    out.write((char *) &magic, 4);
    out.write((char *) &version, 4);
    out.write((char *) &ndx_type, 4);
    out.write((char *) &reserved, 4);

    auto *offsets = new uint32_t[NUM_TRIGRAMS + 1];

    for (int i = 0; i < NUM_TRIGRAMS; i++) {
        offsets[i] = (uint32_t) out.tellp();
        compress_run(raw_index[i], out);
    }
    offsets[NUM_TRIGRAMS] = (uint32_t) out.tellp();

    out.write((char *) offsets, (NUM_TRIGRAMS + 1) * 4);
    out.close();

    delete[] offsets;
}
