#include "utils.h"


std::vector<TriGram> get_trigrams(std::ifstream &infile, long insize) {
    std::vector<TriGram> out;

    if (insize < 3) {
        return out;
    }

    uint8_t ringbuffer[3];
    infile.read((char *)ringbuffer, 3);
    int offset = 2;

    while (offset < insize) {
        uint32_t gram3 =
                (ringbuffer[(offset - 2) % 3] << 16U) +
                (ringbuffer[(offset - 1) % 3] << 8U) +
                (ringbuffer[(offset - 0) % 3] << 0U);
        out.push_back(gram3);
        offset += 1;
        infile.read((char *)&ringbuffer[offset % 3], 1);
    }
    return out;
}
