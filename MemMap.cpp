#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include "MemMap.h"


MemMap::MemMap(const std::string &fname) {
    fd = open(fname.c_str(), O_RDONLY, (mode_t) 0600);

    if (fd == -1) {
        throw std::runtime_error("file open error");
    }

    fsize = static_cast<size_t>(lseek(fd, 0, SEEK_END));

    if (fsize == -1) {
        throw std::runtime_error("lseek failed");
    }

    mmap_ptr = (uint8_t *) mmap(nullptr, fsize, PROT_READ, MAP_SHARED, fd, 0);

    if (mmap_ptr == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("mmap failed");
    }
}

MemMap::~MemMap() {
    // FIXME error checking(?)
    munmap(mmap_ptr, fsize);
    close(fd);
}

const uint8_t &MemMap::operator[](const size_t &offset) const {
    return mmap_ptr[offset];
}

const size_t &MemMap::size() const {
    return fsize;
}
