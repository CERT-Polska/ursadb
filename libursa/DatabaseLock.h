#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

class DatasetLock {
    std::string dataset_;

   public:
    DatasetLock(std::string_view dataset) : dataset_(dataset) {}

    const std::string &target() const { return dataset_; }
};

class IteratorLock {
    std::string iterator_;

   public:
    IteratorLock(std::string_view iterator) : iterator_(iterator) {}

    const std::string &target() const { return iterator_; }
};

using DatabaseLock = std::variant<DatasetLock, IteratorLock>;

std::string describe_lock(const DatabaseLock &lock);
