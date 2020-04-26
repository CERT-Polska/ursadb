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

bool TaskSpec::locks_dataset(std::string_view ds_id) const {
    for (const auto &lock : locks_) {
        if (const auto *dslock = std::get_if<DatasetLock>(&lock)) {
            if (dslock->target() == ds_id) {
                return true;
            }
        }
    }
    return false;
}

bool TaskSpec::locks_iterator(std::string_view it_id) const {
    for (const auto &lock : locks_) {
        if (const auto *itlock = std::get_if<IteratorLock>(&lock)) {
            if (itlock->target() == it_id) {
                return true;
            }
        }
    }
    return false;
}
