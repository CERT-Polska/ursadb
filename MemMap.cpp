#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#ifdef __linux__
#include <sys/mman.h>
#elif _WIN32
#include <Windows.h>
#else
#error "Not a supported platform, can't provide memory mapped files."
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include "MemMap.h"


MemMap::MemMap(const std::string &fname) : fname(fname) {
#ifdef __linux__
    fd = open(fname.c_str(), O_RDONLY, (mode_t) 0600);

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

    mmap_ptr = (uint8_t *) mmap(nullptr, fsize, PROT_READ, MAP_SHARED, fd, 0);

    if (mmap_ptr == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("mmap failed");
    }
#elif _WIN32
    SYSTEM_INFO sysinfo = {0};
    ::GetSystemInfo(&sysinfo);
    DWORD cbView = sysinfo.dwAllocationGranularity;

    hfile = ::CreateFile(fname.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_EXISTING, 0, NULL);
    if (hfile == INVALID_HANDLE_VALUE) {
        cleanup();
        throw std::runtime_error("Failed to create mmap");
    }

    LARGE_INTEGER file_size = {0};
    ::GetFileSizeEx(hfile, &file_size);
    const unsigned long long cbFile =
        static_cast<unsigned long long>(file_size.QuadPart);
    fsize = (size_t) cbFile;

    hmap = ::CreateFileMapping(hfile, NULL, PAGE_READONLY, 0, 0, NULL);

    if (hmap == NULL) {
        cleanup();
        throw std::runtime_error("Failed to create file mapping");
    }

    uint8_t *pView = static_cast<uint8_t *>(
        ::MapViewOfFile(hmap, FILE_MAP_READ, 0, 0, 0));

    if (pView == NULL) {
        cleanup();
        throw std::runtime_error("Empty pointer on pView");
    }

    mmap_ptr = pView;
#endif
}

void MemMap::cleanup() {
#ifdef __linux__
    if (mmap_ptr) {
        munmap(mmap_ptr, fsize);
    }

    if (fd) {
        close(fd);
    }
#elif _WIN32
    UnmapViewOfFile(mmap_ptr);

    if (hmap != NULL) {
        ::CloseHandle(hmap);
    }

    if (hfile != INVALID_HANDLE_VALUE) {
        ::CloseHandle(hfile);
    }
#endif
}

MemMap::~MemMap() {
    cleanup();
}
