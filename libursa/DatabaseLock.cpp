#include "DatabaseLock.h"

#include "Utils.h"

std::string describe_lock(const DatabaseLock &lock) {
    return std::visit([](const auto &lck) { return lck.target(); }, lock);
}
