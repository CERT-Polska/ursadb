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
