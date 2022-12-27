#pragma once

#include <ctime>

#include "DatabaseName.h"

class OnDiskIterator {
    DatabaseName name;
    DatabaseName datafile_name;
    uint64_t total_files;
    uint64_t byte_offset;
    uint64_t file_offset;
    std::time_t last_read_time;


    OnDiskIterator(const DatabaseName &name, const DatabaseName &datafile_name,
                   uint64_t total_files, uint64_t byte_offset,
                   uint64_t file_offset,
                   std::time_t last_read_time);

   public:
    const DatabaseName &get_name() const { return name; }
    const DatabaseName &get_data_name() const { return name; }

    void pop(int count, std::vector<std::string> *out);
    void save();
    void drop();

    uint64_t get_byte_offset() const { return byte_offset; }
    uint64_t get_file_offset() const { return file_offset; }
    uint64_t get_total_files() const { return total_files; }
    std::time_t get_last_read_time() const { return last_read_time; }

    void update_offset(uint64_t new_bytes, uint64_t new_files) {
        byte_offset = new_bytes;
        file_offset = new_files;
    }

    static OnDiskIterator load(const DatabaseName &name);

    static void construct(const DatabaseName &location,
                          const DatabaseName &backing_storage, int total_files);
};
