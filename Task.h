#pragma once

#include <string>
#include <vector>

enum class DbChangeType { Insert = 1, Drop = 2, Reload = 3 };

std::string db_change_to_string(DbChangeType change);

class DBChange {
public:
    DbChangeType type;
    std::string obj_name;

    DBChange(const DbChangeType &type, const std::string &obj_name) : type(type), obj_name(obj_name) {}
};

class Task {
  public:
    // task id
    uint64_t id;
    // remote requestor identity
    std::string conn_id;
    // command issued
    std::string request_str;
    // arbitrary number <= work_done
    uint64_t work_estimated;
    // arbitrary number, for example "number of bytes to index" or "number of trigrams to merge"
    uint64_t work_done;
    // miliseconds since epoch, for ETA calculation
    uint64_t epoch_ms;
    // changes done by this task. Will be resolved after task is finished
    std::vector<DBChange> changes;

    Task(uint64_t id, uint64_t epoch_ms, const std::string &request, const std::string &conn_id)
        : id(id), work_estimated(0), work_done(0), epoch_ms(epoch_ms), changes(),
          request_str(request), conn_id(conn_id) {}
};