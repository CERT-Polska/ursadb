#pragma once

#include <string>
#include <vector>

#include "Json.h"
#include "Core.h"
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
    std::vector<IndexEntry> indexes;
};

class Response {
    json content;

    Response(const std::string &type) {
        content["type"] = type;
    }

public:
    static Response select(const std::vector<std::string> &files);
    static Response ok();
    static Response ping(const std::string &connection_id);
    static Response error(const std::string &message);
    static Response topology(const std::vector<DatasetEntry> &datasets);
    static Response status(const std::vector<TaskEntry> &tasks);
    std::string to_string() const;
};
