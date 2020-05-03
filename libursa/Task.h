#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "DatabaseLock.h"
#include "Utils.h"

enum class DbChangeType {
    Insert = 1,
    Drop = 2,
    Reload = 3,
    ToggleTaint = 4,
    NewIterator = 5,
    UpdateIterator = 6,
    ConfigChange = 7,
};

std::string db_change_to_string(DbChangeType change);

class DBChange {
   public:
    DbChangeType type;
    std::string obj_name;
    std::string parameter;

    DBChange(const DbChangeType &type, const std::string &obj_name,
             const std::string &parameter = "")
        : type(type), obj_name(obj_name), parameter(parameter) {}
};

// Shared task object. This class can be read by multiple threads at once, but
// only one thread can write (either worker task or controller reaping results).
// Most of the fields are immutable after creation
class TaskSpec {
    // Task id.
    const uint64_t id_;
    // Remote requestor identity.
    const std::string conn_id_;
    // Command issued.
    const std::string request_str_;
    // miliseconds since epoch, for ETA calculation
    const uint64_t epoch_ms_;
    // Arbitrary number, for example "number of bytes to index" or "number of
    // trigrams to merge".
    std::atomic_uint64_t work_estimated_;
    // Arbitrary number lesser than work_estimated.
    std::atomic_uint64_t work_done_;
    // Immutable vector of locks acquired by this task.
    std::vector<DatabaseLock> locks_;

    // Helpers for std::visit
    bool has_typed_lock(const DatasetLock &other) const;
    bool has_typed_lock(const IteratorLock &other) const;

   public:
    TaskSpec(uint64_t id, std::string conn_id, std::string request_str,
             uint64_t epoch_ms, std::vector<DatabaseLock> locks)
        : id_(id),
          conn_id_(std::move(conn_id)),
          request_str_(std::move(request_str)),
          epoch_ms_(epoch_ms),
          work_estimated_(0),
          work_done_(0),
          locks_(locks) {}

    TaskSpec(const TaskSpec &oth)
        : id_(oth.id_),
          conn_id_(oth.conn_id_),
          request_str_(oth.request_str_),
          epoch_ms_(oth.epoch_ms_),
          work_estimated_(oth.work_estimated_.load()),
          work_done_(oth.work_done_.load()),
          locks_(oth.locks_) {}

    uint64_t id() const { return id_; }
    const std::string &request_str() const { return request_str_; }
    uint64_t epoch_ms() const { return epoch_ms_; }
    uint64_t work_estimated() const { return work_estimated_; }
    uint64_t work_done() const { return work_done_; }
    const std::vector<DatabaseLock> &locks() const { return locks_; };

    bool has_lock(const DatabaseLock &oth) const;

    std::string hex_conn_id() const { return bin_str_to_hex(conn_id_); }

    void estimate_work(uint64_t new_estimation) {
        work_estimated_ = new_estimation;
    }

    void set_work(uint64_t new_work) { work_done_ = new_work; }

    void add_progress(uint64_t done_units) { work_done_ += done_units; }
};

// Thread-local task object. Must not be shared by multiple threads because of
// potentially thread-unsafe collections used.
class Task {
   public:
    // Immutable specification of this task.
    TaskSpec *spec_;
    std::vector<DBChange> changes_;

    Task(TaskSpec *spec) : spec_(spec), changes_{} {}

    void change(DBChange change) { changes_.emplace_back(std::move(change)); }

    const std::vector<DBChange> changes() const { return changes_; }

    TaskSpec &spec() { return *spec_; }
};
