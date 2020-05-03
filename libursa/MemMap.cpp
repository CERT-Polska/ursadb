#include "MemMap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

MemMap::MemMap(const std::string &fname) : fname(fname) {
    fd = open(fname.c_str(), O_RDONLY, (mode_t)0600);

    if (fd == -1) {
        throw file_open_error("file open error");
    }

    off_t fsize_tmp = lseek(fd, 0, SEEK_END);

    if (fsize_tmp == 0) {
        close(fd);
        throw empty_file_error(fname);
    }

    if (fsize_tmp == -1) {
        close(fd);
        throw file_open_error("lseek failed");
    }

    fsize = static_cast<uint64_t>(fsize_tmp);
    mmap_ptr = (uint8_t *)mmap(nullptr, fsize, PROT_READ, MAP_SHARED, fd, 0);

    if (mmap_ptr == MAP_FAILED) {
        close(fd);
        throw file_open_error("mmap failed");
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
    if (mmap_ptr != nullptr) {
        munmap(mmap_ptr, fsize);
    }

    if (fd != -1 && fd != 0) {
        close(fd);
    }
}

const char *empty_file_error::what() const _GLIBCXX_TXN_SAFE_DYN
    _GLIBCXX_USE_NOEXCEPT {
    return what_message.c_str();
}

const char *file_open_error::what() const _GLIBCXX_TXN_SAFE_DYN
    _GLIBCXX_USE_NOEXCEPT {
    return what_message.c_str();
}
