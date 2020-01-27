#include "FlatIndexBuilder.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <cassert>

#include "Utils.h"

constexpr int MAX_TRIGRAMS = 100000000;

FlatIndexBuilder::FlatIndexBuilder(IndexType ntype)
    : IndexBuilder(ntype), raw_data() {
    raw_data.reserve(MAX_TRIGRAMS);
}

void FlatIndexBuilder::add_trigram(FileId fid, TriGram val) {
    raw_data.push_back((fid & 0xFFU) | (val << 8U));
}

void FlatIndexBuilder::save(const std::string &fname) {
    std::ofstream out;
    out.exceptions(std::ofstream::badbit);
    out.open(fname, std::ofstream::binary);

    uint32_t magic = DB_MAGIC;
    uint32_t version = 6;
    uint32_t ndx_type = static_cast<uint32_t>(index_type());
    uint32_t reserved = 0;

    out.write((char *)&magic, 4);
    out.write((char *)&version, 4);
    out.write((char *)&ndx_type, 4);
    out.write((char *)&reserved, 4);

    auto offset = (uint64_t)out.tellp();
    std::vector<uint64_t> offsets(NUM_TRIGRAMS + 1);
    offsets[0] = offset;

    std::sort(raw_data.begin(), raw_data.end());
    raw_data.erase(std::unique(raw_data.begin(), raw_data.end()), raw_data.end());

    TriGram last_trigram = 0;
    int64_t prev = -1;

    for (const uint32_t &d : raw_data) {
        TriGram val = (d >> 8U) & (0xFFFFFFU);
        FileId next = (d & 0xFFU);

        // adjust offsets for [last_trigram+1, val)
        if (last_trigram != val) {
            for (TriGram v = last_trigram + 1; v <= val; v++) {
                offsets[v] = offset;
            }

            last_trigram = val;
            prev = -1;
        }

        int64_t diff = (next - prev) - 1;
        while (diff >= 0x80U) {
            offset++;
            out.put((uint8_t)(0x80U | (diff & 0x7FU)));
            diff >>= 7;
        }

        offset++;
        out.put((uint8_t)diff);
        prev = next;
    }

    for (TriGram v = last_trigram + 1; v <= NUM_TRIGRAMS; v++) {
        offsets[v] = offset;
    }

    out.write((char *)offsets.data(), (NUM_TRIGRAMS + 1) * sizeof(uint64_t));
}

void FlatIndexBuilder::add_file(FileId fid, const uint8_t *data, size_t size) {
    TrigramGenerator generator = get_generator_for(index_type());
    generator(data, size, [&](TriGram val) { add_trigram(fid, val); });
}

bool FlatIndexBuilder::must_spill(int file_count) const {
    return raw_data.size() > 90000000;
}
