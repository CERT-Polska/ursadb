#include "Utils.h"
#include "MemMap.h"

std::vector<TriGram> get_trigrams(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    if (size < 3) {
        return out;
    }

    for (int offset = 2; offset < size; offset++) {
        uint32_t gram3 = (mem[offset - 2] << 16U) + (mem[offset - 1] << 8U) + (mem[offset] << 0U);
        out.push_back(gram3);
    }

    return out;
}

void compress_run(const std::vector<FileId> &run, std::ofstream &out) {
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

std::vector<FileId> read_compressed_run(const uint8_t *start, const uint8_t *end) {
    std::vector<FileId> res;
    uint32_t acc = 0;
    uint32_t shift = 0;
    uint32_t base = 0;

    for (const uint8_t *ptr = start; ptr < end; ++ptr) {
        uint32_t next = *ptr;

        acc += (next & 0x7FU) << shift;
        shift += 7U;
        if ((next & 0x80U) == 0) {
            base += acc;
            res.push_back(base - 1U);
            acc = 0;
            shift = 0;
        }
    }

    return res;
}