#include <sys/syscall.h>
#include <unistd.h>

#include "IoPrio.h"
#include "spdlog/spdlog.h"

// Reference: https://www.kernel.org/doc/html/latest/block/ioprio.html

static inline int ioprio_set(int which, int who, int ioprio) {
    return syscall(__NR_ioprio_set, which, who, ioprio);
}

static inline int ioprio_get(int which, int who) {
    return syscall(__NR_ioprio_get, which, who);
}

// Copied from include/linux/ioprio.h
enum {
    IOPRIO_WHO_PROCESS = 1,
    IOPRIO_WHO_PGRP,
    IOPRIO_WHO_USER,
};

enum {
    IOPRIO_CLASS_NONE,
    IOPRIO_CLASS_RT,
    IOPRIO_CLASS_BE,
    IOPRIO_CLASS_IDLE,
};

constexpr int IOPRIO_CLASS_SHIFT = 13;

// Change ioprio for the current process, and save the current PID.
IoPriority::IoPriority(IoPriorityClass ioprio) {
    _old_pid = getpid();
    _old_ioprio = ioprio_get(IOPRIO_WHO_PROCESS, _old_pid);
    int ioprio_value = 0;
    if (ioprio == IoPriorityClass::BestEffort) {
        ioprio_value = (IOPRIO_CLASS_BE << IOPRIO_CLASS_SHIFT) | 0;
    } else if (ioprio == IoPriorityClass::Idle) {
        ioprio_value = (IOPRIO_CLASS_IDLE << IOPRIO_CLASS_SHIFT) | 7;
    } else {
        throw std::runtime_error("Invalid IoPriorityClass");
    }
    ioprio_set(IOPRIO_WHO_PROCESS, _old_pid, ioprio_value);
}

// Restore ioprio for the current process (and check it's still the same PID).
IoPriority::~IoPriority() {
    pid_t current_pid = getpid();
    ioprio_set(IOPRIO_WHO_PROCESS, current_pid, _old_ioprio);
    if (current_pid != _old_pid) {
        // This should not happen. Maybe we should just raise an error,
        // but instead let's just try to the right thing and keep going.
        spdlog::error("Thread forked in IoPriority context.");
    }
}
