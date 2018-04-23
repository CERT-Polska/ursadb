#pragma once

#include <string>

class ExclusiveFile {
  public:
    ExclusiveFile(const std::string &path);
    ~ExclusiveFile();
    bool is_ok();
    int get_fd();

  private:
    int fd;
};