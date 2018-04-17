#include "MemMap.h"

#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

MemMap::MemMap(const std::string &fname) : fname(fname) {
    fd = open(fname.c_str(), O_RDONLY, (mode_t)0600);

    if (fd == -1) {
        throw std::runtime_error("file open error");
    }

    fsize = static_cast<size_t>(lseek(fd, 0, SEEK_END));

    if (fsize == 0) {
        close(fd);
        throw empty_file_error(fname);
    }

    if (fsize == -1) {
        close(fd);
        throw std::runtime_error("lseek failed");
    }

    mmap_ptr = (uint8_t *)mmap(nullptr, fsize, PROT_READ, MAP_SHARED, fd, 0);

    if (mmap_ptr == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("mmap failed");
    }
}

MemMap::MemMap(MemMap &&other) {
    this->fd = other.fd;
    this->fname = other.fname;
    this->fsize = other.fsize;
    this->mmap_ptr = other.mmap_ptr;

    other.mmap_ptr = 0;
    other.fd = -1;
}

MemMap::~MemMap() {
    if (mmap_ptr) {
        munmap(mmap_ptr, fsize);
    }

    if (fd != -1 && fd != 0) {
        close(fd);
    }
}
