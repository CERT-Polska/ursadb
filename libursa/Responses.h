#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core.h"
#include "Json.h"
#include "QueryResult.h"
#include "Task.h"

struct IndexEntry {
    IndexType type;
    unsigned long size;
};

struct DatasetEntry {
    std::string id;
    unsigned long size;
    unsigned long file_count;
    std::set<std::string> taints;
    std::vector<IndexEntry> indexes;
};

class Response {
    json content;

    Response(const std::string &type) { content["type"] = type; }

    void write_counters(
        const std::unordered_map<std::string, QueryCounter> &counters);

   public:
    static Response select(
        const std::vector<std::string> &files,
        const std::unordered_map<std::string, QueryCounter> &counters);
    static Response select_iterator(
        const std::string &iterator, uint64_t file_count,
        const std::unordered_map<std::string, QueryCounter> &counters);
    static Response select_from_iterator(const std::vector<std::string> &files,
                                         uint64_t iterator_position,
                                         uint64_t total_files);
    static Response ok();
    static Response config(std::unordered_map<std::string, uint64_t> values);
    static Response ping(const std::string &connection_id);
    static Response error(const std::string &message, bool retry = false);
    static Response topology(const std::vector<DatasetEntry> &datasets);
    static Response status(const std::unordered_map<uint64_t, TaskSpec> &tasks);
    std::string to_string() const;
};
