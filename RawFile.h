#pragma once

#include <fstream>

class RawFile {
    int fd;

public:
    explicit RawFile(const std::string &fname);
    RawFile(RawFile &&);
    ~RawFile();

    uint64_t size() const;
    void pread(void *buf, size_t count, off_t offset) const;
};
