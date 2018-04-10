#include <iostream>
#include "IndexBuilder.h"


IndexBuilder::~IndexBuilder() {
    for (auto it : raw_index) {
        while (it) {
            auto *prev = it;
            it = it->next;
            delete prev;
        }
    }
}

void IndexBuilder::add_trigram(const FileId &fid, const TriGram &val) {
    auto *entry = new LinkedFile(fid);

    if (raw_index[val] == nullptr) {
        raw_index[val] = entry;
    } else {
        entry->next = raw_index[val];
        raw_index[val] = entry;
    }
}

void IndexBuilder::compress_run(LinkedFile *run, std::ofstream &out) {
    uint32_t prev = 0;

    for (; run; run = run->next) {
        uint32_t diff = (run->fid + 1U) - prev;
        while (diff >= 0x80U) {
            out.put((uint8_t)(0x80U | (diff & 0x7FU)));
            diff >>= 7;
        }
        out.put((uint8_t) diff);
        prev = run->fid + 1U;
    }
}

void IndexBuilder::save(const std::string &fname) {
    std::ofstream out(fname, std::ofstream::binary);

    uint32_t magic = 0xCA7DA7A;
    uint32_t version = 5;
    uint32_t ndx_type = 1;
    uint32_t reserved = 0;

    out.write((char*)&magic, 4);
    out.write((char*)&version, 4);
    out.write((char*)&ndx_type, 4);
    out.write((char*)&reserved, 4);

    auto *offsets = new uint32_t[NUM_TRIGRAMS];

    for (int i = 0; i < NUM_TRIGRAMS; i++) {
        offsets[i] = (uint32_t)out.tellp();
        // FIXME std::cout << i << " " << offsets[i] << std::endl;
        compress_run(raw_index[i], out);
    }

    out.write((char *) offsets, NUM_TRIGRAMS*4);
    out.close();

    delete[] offsets;
}
