#pragma once

#include <iostream>
#include <vector>
#include <fstream>

#include "Core.h"
#include "MemMap.h"

template <typename T>
std::vector<TriGram> get_trigrams(const T &mem, size_t size) {
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
