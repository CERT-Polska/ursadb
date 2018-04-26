#pragma once

class Task {
  public:
    // unique task id
    uint64_t id;
    // arbitrary number <= work_done
    uint64_t work_estimated;
    // arbitrary number, for example "number of bytes to index" or "number of trigrams to merge"
    uint64_t work_done;
    // for ETA calculation
    time_t start_timestamp;

    Task(uint64_t id) : id(id), work_estimated(0), work_done(0), start_timestamp(time(nullptr)) {}
};