#pragma once

using DBChange = std::pair<std::string, std::string>;

class Task {
  public:
    // task id
    uint64_t id;
    // arbitrary number <= work_done
    uint64_t work_estimated;
    // arbitrary number, for example "number of bytes to index" or "number of trigrams to merge"
    uint64_t work_done;
    // miliseconds since epoch, for ETA calculation
    time_t epoch_ms;

    std::vector<DBChange> changes;

    Task(uint64_t id, uint64_t epoch_ms) : id(id), work_estimated(0), work_done(0), epoch_ms(epoch_ms), changes() {}
};