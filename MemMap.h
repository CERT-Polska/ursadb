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
    const uint8_t &operator[](const size_t &offset) const;
    const size_t &size() const;
};
