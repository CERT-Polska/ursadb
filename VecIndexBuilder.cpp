#include "VecIndexBuilder.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <cassert>

#include "Utils.h"

constexpr int reserve_ints = 1024;
constexpr int max_files = 256;


VecIndexBuilder::VecIndexBuilder(IndexType ntype)
    : added_trigrams(), raw_data(), ntype(ntype) {
    raw_data.reserve(reserve_ints);
}

void VecIndexBuilder::add_trigram(FileId fid, TriGram val) {
    if (!added_trigrams[val]) {
        added_trigrams[val] = true;
        raw_data.push_back((fid & 0xFFU) | (val << 8U));
    }
}

void VecIndexBuilder::save(const std::string &fname) {
    std::ofstream out(fname, std::ofstream::binary | std::ofstream::out);

    uint32_t magic = DB_MAGIC;
    uint32_t version = 6;
    uint32_t ndx_type = static_cast<uint32_t>(ntype);
    uint32_t reserved = 0;

    out.write((char *)&magic, 4);
    out.write((char *)&version, 4);
    out.write((char *)&ndx_type, 4);
    out.write((char *)&reserved, 4);

    auto offset = (uint64_t)out.tellp();
    std::vector<uint64_t> offsets(NUM_TRIGRAMS + 1);
    offsets[0] = offset;

    std::sort(raw_data.begin(), raw_data.end());

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

void VecIndexBuilder::add_file(FileId fid, const uint8_t *data, size_t size) {
    if (fid >= max_files) {
        // IndexBuilder's bitmap can't hold more than max_files files
        throw std::out_of_range("fid");
    }

    std::fill(added_trigrams.begin(), added_trigrams.end(), 0);
    TrigramGenerator generator = get_generator_for(ntype);
    generator(data, size, [&](TriGram val) { add_trigram(fid, val); });
}

bool VecIndexBuilder::must_spill(int file_count) const {
    return file_count >= max_files;
}