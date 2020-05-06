#include "Environment.h"

#include <sys/resource.h>

#include "spdlog/spdlog.h"

// Increase RLIMIT to something more reasonable.
void fix_rlimit() {
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
        spdlog::warn("getrlimit() failed");
        return;
    }

    limit.rlim_cur = limit.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
        spdlog::warn("setrlimit() failed");
        return;
    }

    spdlog::debug("RLIMIT_NOFILE updated: {}", limit.rlim_cur);
}
