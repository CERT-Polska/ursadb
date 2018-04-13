#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

class MemMap {
#ifdef __linux__
    int fd;
#elif _WIN32
    HANDLE hfile;
    HANDLE hmap;
#endif
    uint8_t *mmap_ptr;
    size_t fsize;

    void cleanup();

public:
    explicit MemMap(const std::string &fname);
    constexpr MemMap(MemMap &&other) = default;
    ~MemMap();

    // Disables copy constructor - we DO NOT want to accidentaly copy MemMap object
    MemMap(const MemMap &other) = delete;

    const uint8_t *data() const {
        return mmap_ptr;
    }

    const size_t &size() const {
        return fsize;
    }
};
