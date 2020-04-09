#include "FlatIndexBuilder.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>

#include "Utils.h"

// raw_data will occupy at most 762 MB (MAX_TRIGRAMS*8/1024/1024)
// TODO(): this should be parametrised by user somehow.
constexpr int MAX_TRIGRAMS = 100000000;

FlatIndexBuilder::FlatIndexBuilder(IndexType ntype)
    : IndexBuilder(ntype), raw_data() {
    raw_data.reserve(MAX_TRIGRAMS);
}

void FlatIndexBuilder::add_trigram(FileId fid, TriGram val) {
    // Consider packing the data in more memory efficient ways
    // At least if it's worth it (TODO: measure performance).
    raw_data.push_back(fid | (uint64_t{val} << 40ULL));
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

    // Sort raw_data by trigrams (higher part of raw_data contains the
    // trigram value and lower part has the file ID).
    std::sort(raw_data.begin(), raw_data.end());

    // Remove the duplicates (Files will often contain duplicated trigrams).
    raw_data.erase(std::unique(raw_data.begin(), raw_data.end()),
                   raw_data.end());

    TriGram last_trigram = 0;
    int64_t prev = -1;

    for (uint64_t d : raw_data) {
        TriGram val = (d >> 40ULL) & 0xFFFFFFU;
        FileId next = d & 0xFFFFFFFFFFULL;

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

bool FlatIndexBuilder::can_still_add(uint64_t bytes, int file_count) const {
    uint64_t max_number_of_trigrams_produced = bytes - 2;
    uint64_t max_trigrams_after_add =
        raw_data.size() + max_number_of_trigrams_produced;
    return max_trigrams_after_add < MAX_TRIGRAMS;
}
