#include <iostream>
#include "IndexBuilder.h"


IndexBuilder::IndexBuilder() : raw_index(NUM_TRIGRAMS) {

}

void IndexBuilder::add_trigram(FileId fid, TriGram val) {
    raw_index[val].push_back(fid);
}

void IndexBuilder::compress_run(const std::vector<FileId> &run, std::ofstream &out) {
    uint32_t prev = 0;

    for (auto next : run) {
        uint32_t diff = (next + 1U) - prev;
        while (diff >= 0x80U) {
            out.put((uint8_t) (0x80U | (diff & 0x7FU)));
            diff >>= 7;
        }
        out.put((uint8_t) diff);
        prev = next + 1U;
    }
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

    auto *offsets = new uint32_t[NUM_TRIGRAMS];

    for (int i = 0; i < NUM_TRIGRAMS; i++) {
        offsets[i] = (uint32_t) out.tellp();
        compress_run(raw_index[i], out);
    }

    out.write((char *) offsets, NUM_TRIGRAMS * 4);
    out.close();

    delete[] offsets;
}
