#include <sys/stat.h>
#include <unistd.h>

#include "OnDiskIndex.h"

RawFile::RawFile(const std::string &fname, int flags, int mode) {
    fd = ::open(fname.c_str(), flags, mode);
    if (fd < 0) {
        std::string message;
        message += "RawFile::RawFile: open for " + fname + " failed";
        throw std::runtime_error(message);
    }
}

RawFile::RawFile(RawFile &&other) {
    fd = other.fd;
    other.fd = -1;
}

RawFile::~RawFile() {
    if (fd != -1) {
        close(fd);
    }
}

uint64_t RawFile::size() const {
    struct stat st;
    if (::fstat(fd, &st) != 0) {
        throw std::runtime_error("RawFile::size: fstat failed");
    }
    return st.st_size;
}

void RawFile::pread(void *buf, size_t to_read, off_t offset) const {
    char *buf_raw = static_cast<char *>(buf);
    while (to_read > 0) {
        ssize_t result = ::pread(fd, buf_raw, to_read, offset);
        if (result < 0) {
            throw std::runtime_error("RawFile::pread: pread failed");
        }
        buf_raw += result;
        offset += result;
        to_read -= result;
    }
}

template <typename T>
void RawFile::write(const T *buf, size_t count) {
    const char *buf_raw = reinterpret_cast<const char *>(buf);
    uint64_t to_write = count * sizeof(T);
    while (to_write > 0) {
        ssize_t result = ::write(fd, buf_raw, to_write);
        if (result < 0) {
            throw std::runtime_error("RawFile::write: write failed");
        }
        buf_raw += result;
        to_write -= result;
    }
}

template void RawFile::write(const uint8_t *, size_t);
template void RawFile::write(const uint64_t *, size_t);
