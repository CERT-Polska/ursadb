#pragma once

class Task {
public:
    uint64_t id; // unique task id
    uint64_t work_estimated; // arbitrary number <= work_done
    uint64_t work_done; // arbitrary number, for example "number of bytes to index" or "number of trigrams to merge"
    time_t start_timestamp; // for ETA calculation

    Task(uint64_t id) : id(id), work_estimated(0), work_done(0), start_timestamp(time(nullptr)) {}
};