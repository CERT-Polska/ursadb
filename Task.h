#pragma once

#include <string>
#include <vector>

enum class DbChangeType { Insert = 1, Drop = 2 };

std::string db_change_to_string(DbChangeType change);

using DBChange = std::pair<DbChangeType, std::string>;

class Task {
  public:
    // task id
    uint64_t id;
    // arbitrary number <= work_done
    uint64_t work_estimated;
    // arbitrary number, for example "number of bytes to index" or "number of trigrams to merge"
    uint64_t work_done;
    // miliseconds since epoch, for ETA calculation
    uint64_t epoch_ms;
    // changes done by this task. Will be resolved after task is finished
    std::vector<DBChange> changes;

    Task(uint64_t id, uint64_t epoch_ms)
        : id(id), work_estimated(0), work_done(0), epoch_ms(epoch_ms), changes() {}
};