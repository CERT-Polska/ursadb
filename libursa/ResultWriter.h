#pragma once

#include <fstream>

// Abstract class used to update results of the select() query in a
// generic way. Can be used to save results into a file, or to
// keep them in memory.
class ResultWriter {
   public:
    virtual ~ResultWriter() = default;
    virtual void push_back(const std::string &filename) = 0;
};

class InMemoryResultWriter : public ResultWriter {
    std::vector<std::string> out;

   public:
    InMemoryResultWriter() : out() {}

    const std::vector<std::string> &get() const { return out; }

    virtual void push_back(const std::string &filename) {
        out.push_back(filename);
    }
};

class FileResultWriter : public ResultWriter {
    std::ofstream out;
    uint64_t file_count;

   public:
    FileResultWriter(const std::string path)
        : out(path, std::ios_base::out | std::ios_base::binary),
          file_count(0) {}

    virtual void push_back(const std::string &filename) {
        out << filename << "\n";
        file_count += 1;
    }

    uint64_t get_file_count() { return file_count; }
};
