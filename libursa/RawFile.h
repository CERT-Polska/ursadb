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
    void prefetch(size_t size, off_t offse) const;

    template <typename T>
    void write(const T *buf, size_t count);

    int get() { return fd; }
};
