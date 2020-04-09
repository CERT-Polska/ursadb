#pragma once

#include <string>
#include <vector>

#include "Core.h"
#include "Json.h"
#include "Utils.h"

struct TaskEntry {
    std::string id;
    std::string connection_id;
    std::string request;
    uint64_t work_done;
    uint64_t work_estimated;
    uint64_t epoch_ms;
};

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

   public:
    static Response select(const std::vector<std::string> &files);
    static Response select_iterator(const std::string &filename,
                                    uint64_t file_count);
    static Response select_from_iterator(const std::vector<std::string> &files,
                                         uint64_t iterator_position,
                                         uint64_t total_files);
    static Response ok();
    static Response ping(const std::string &connection_id);
    static Response error(const std::string &message, bool retry = false);
    static Response topology(const std::vector<DatasetEntry> &datasets);
    static Response status(const std::vector<TaskEntry> &tasks);
    std::string to_string() const;
};
