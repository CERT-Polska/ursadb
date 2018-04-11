#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

class MemMap {
    int fd;
    uint8_t *mmap_ptr;
    size_t fsize;

public:
    explicit MemMap(const std::string &fname);
    ~MemMap();

    const uint8_t *data() const {
        return mmap_ptr;
    }

    const size_t &size() const {
        return fsize;
    }
};
