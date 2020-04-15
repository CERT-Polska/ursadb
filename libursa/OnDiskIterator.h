#pragma once

#include "DatabaseName.h"

class OnDiskIterator {
    DatabaseName name;
    std::string datafile_filename;
    uint64_t total_files;
    uint64_t byte_offset;
    uint64_t file_offset;

   public:
    explicit OnDiskIterator(const DatabaseName &name);

    const DatabaseName &get_name() const { return name; }

    void pop(int count, std::vector<std::string> *out);
    void save();
    void drop();

    uint64_t get_byte_offset() const { return byte_offset; }

    uint64_t get_file_offset() const { return file_offset; }

    uint64_t get_total_files() const { return total_files; }

    void update_offset(uint64_t new_bytes, uint64_t new_files) {
        byte_offset = new_bytes;
        file_offset = new_files;
    }

    static void construct(const DatabaseName &location,
                          const DatabaseName &backing_storage, int total_files);
};
