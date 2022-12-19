#include "ExclusiveFile.h"

#include <fcntl.h>
#include <unistd.h>

#include <stdexcept>

ExclusiveFile::ExclusiveFile(const std::string &path) {
    fd = open(path.c_str(), O_CREAT | O_EXCL, 0666);
}

ExclusiveFile::~ExclusiveFile() {
    if (fd != -1) {
        close(fd);
    }
}

bool ExclusiveFile::is_ok() { return fd != -1; }

int ExclusiveFile::get_fd() {
    if (fd == -1) {
        throw std::runtime_error(
            "Trying to extract descriptor from failed open()");
    }
    return fd;
}
