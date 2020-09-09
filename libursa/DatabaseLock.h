#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

// Represents a lock of a single dataset. Dataset can be locked by only one
// task at once.
class DatasetLock {
    std::string dataset_;

   public:
    DatasetLock(std::string_view dataset) : dataset_(dataset) {}

    const std::string &target() const { return dataset_; }
};

// Represents a lock of a single iterator. Iterator can be locked by only one
// task at once.
class IteratorLock {
    std::string iterator_;

   public:
    IteratorLock(std::string_view iterator) : iterator_(iterator) {}

    const std::string &target() const { return iterator_; }
};

// Represents a lock of N mebibytes (1 MiB = 2**20 bytes = a bit more than 1MB).
// Tasks should lock enough memory to work in a worst case, though it's not
// strictly checked (especially for queries with many wildcards).
class MemoryLock {
    uint64_t mebibytes_;
    const std::string description_;

   public:
    MemoryLock(uint64_t mebibytes)
        : mebibytes_(mebibytes),
          description_(std::to_string(mebibytes) + "MiB") {}

    // Amount of memory locked by this task
    uint64_t mebibytes() const { return mebibytes_; }

    // Used for human-friendly description of this lock.
    const std::string &target() const { return description_; }
};

using DatabaseLock = std::variant<DatasetLock, IteratorLock, MemoryLock>;

std::string describe_lock(const DatabaseLock &lock);
