#include "OnDiskIndex.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

RawFile::RawFile(const std::string &fname) {
    fd = ::open(fname.c_str(), O_RDONLY);
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
    if (::fstat(fd, &st)) {
        throw std::runtime_error("RawFile::size: fstat failed");
    }
    return st.st_size;
}

void RawFile::pread(void *buf, size_t count, off_t offset) const {
    if (::pread(fd, buf, count, offset) != static_cast<ssize_t>(count)) {
        throw std::runtime_error("RawFile::pread: pread failed");
    }
}

