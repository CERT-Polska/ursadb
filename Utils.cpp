#include "Utils.h"
#include "MemMap.h"


std::vector<TriGram> get_trigrams(const MemMap &infile) {
    std::vector<TriGram> out;

    if (infile.size() < 3) {
        return out;
    }

    for (int offset = 2; offset < infile.size(); offset++) {
        uint32_t gram3 = (infile[offset - 2] << 16U) + (infile[offset - 1] << 8U) + (infile[offset] << 0U);
        out.push_back(gram3);
    }

    return out;
}
