#include "Environment.h"

#include <sys/resource.h>

#include "spdlog/spdlog.h"

// On some systems the default limit of open files for service is very low.
// Increase it to something more reasonable.
void fix_rlimit() {
    struct rlimit limit;

    limit.rlim_cur = 65535;
    limit.rlim_max = 65535;
    if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
        spdlog::warn("setrlimit() failed");
    }
}
