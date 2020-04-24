#pragma once

#include <fcntl.h>

class RawFile {
    int fd;

   public:
    explicit RawFile(const std::string &fname, int flags = O_RDONLY,
                     int mode = 0);
    RawFile(RawFile &&);
    ~RawFile();

    uint64_t size() const;
    void pread(void *buf, size_t to_read, off_t offset) const;
    void write(void *buf, size_t to_write) const;

    int get() { return fd; }
};
