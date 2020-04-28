#include "Task.h"

std::string db_change_to_string(DbChangeType change) {
    switch (change) {
        case DbChangeType::Insert:
            return "INSERT";
        case DbChangeType::Drop:
            return "DROP";
        case DbChangeType::Reload:
            return "RELOAD";
        case DbChangeType::ConfigChange:
            return "CONFIG_CHANGE";
        case DbChangeType::ToggleTaint:
            return "TOGGLE_TAINT";
        case DbChangeType::NewIterator:
            return "NEW_ITERATOR";
        case DbChangeType::UpdateIterator:
            return "UPDATE_ITERATOR";
    }
    return "<invalid?>";
}

bool TaskSpec::has_typed_lock(const DatasetLock &other) const {
    for (const auto &lock : locks_) {
        if (const auto *dslock = std::get_if<DatasetLock>(&lock)) {
            if (dslock->target() == other.target()) {
                return true;
            }
        }
    }
    return false;
}

bool TaskSpec::has_typed_lock(const IteratorLock &other) const {
    for (const auto &lock : locks_) {
        if (const auto *itlock = std::get_if<IteratorLock>(&lock)) {
            if (itlock->target() == other.target()) {
                return true;
            }
        }
    }
    return false;
}

bool TaskSpec::has_lock(const DatabaseLock &oth) const {
    return std::visit([this](const auto &l) { return has_typed_lock(l); }, oth);
}
