#include "Utils.h"
#include "MemMap.h"

TrigramGenerator get_generator_for(IndexType type) {
    switch (type) {
        case IndexType::GRAM3: return get_trigrams;
        case IndexType::TEXT4: return get_b64grams;
    }
}

constexpr int get_b64_value(uint8_t character) {
    constexpr int ALPHABET_SIZE = 'Z' - 'A' + 1;
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    } else if (character >= 'a' && character <= 'z') {
        return character - 'a' + ALPHABET_SIZE;
    } else if (character >= '0' && character <= '9') {
        return character - '0' + (2 * ALPHABET_SIZE);
    } else if (character == ' ') {
        return 2 * ALPHABET_SIZE + 10;
    } else if (character == '\n') {
        return 2 * ALPHABET_SIZE + 10 + 1;
    } else {
        return -1;
    }
}

std::vector<TriGram> get_b64grams(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    if (size < 4) {
        return out;
    }

    uint32_t gram4 = 0;
    int good_run = 0;

    for (int offset = 3; offset < size; offset++) {
        int next = get_b64_value(mem[offset]);
        if (next < 0) {
            good_run = 0;
        } else {
            gram4 = ((gram4 << 6) + next) & 0xFFFFFF;
            good_run += 1;
        }
        if (good_run >= 4) {
            out.push_back(gram4);
        }
    }

    return out;
}

std::vector<TriGram> get_trigrams(const uint8_t *mem, size_t size) {
    std::vector<TriGram> out;

    if (size < 3) {
        return out;
    }

    uint32_t gram3 = (mem[0] << 8U) | mem[1];

    for (int offset = 2; offset < size; offset++) {
        gram3 = ((gram3 & 0xFFFFU) << 8U) | mem[offset];
        out.push_back(gram3);
    }

    return out;
}

void compress_run(const std::vector<FileId> &run, std::ostream &out) {
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

std::string get_index_type_name(IndexType type) {
    switch (type) {
        case IndexType::GRAM3:
            return "gram3";
        case IndexType::TEXT4:
            return "text4";
    }
}