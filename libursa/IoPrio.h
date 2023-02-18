#include <sys/syscall.h>
#include <unistd.h>

// Strongly typed wrapper for IOPRIO enum in the Linux kernel.
enum class IoPriorityClass { Idle, BestEffort };

// RAII class for setting the current thread's IO priority.
// Will change thread's ioprio in the constructor, and restore in destructor.
class IoPriority {
    int _old_ioprio;
    pid_t _old_pid;

   public:
    IoPriority(IoPriorityClass ioprio);
    ~IoPriority();
};
