#include <sys/syscall.h>
#include <unistd.h>

// Copied from include/linux/ioprio.h
enum {
    IOPRIO_CLASS_NONE,
    IOPRIO_CLASS_RT,
    IOPRIO_CLASS_BE,
    IOPRIO_CLASS_IDLE,
};

// Strongly typed wrapper for IOPRIO enum in the Linux kernel.
enum class IoPriorityClass {
    Idle = IOPRIO_CLASS_IDLE,
    BestEffort = IOPRIO_CLASS_BE
    // IOPRIO_CLASS_RT not needed, and not defined intentionally.
};

// RAII class for setting the current thread's IO priority.
// Will change thread's ioprio in the constructor, and restore in destructor.
class IoPriority {
    int _old_ioprio;
    pid_t _old_pid;

   public:
    IoPriority(IoPriorityClass ioprio);
    ~IoPriority();
};
