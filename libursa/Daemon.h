#pragma once

#include <string>
#include <vector>

#include "Command.h"
#include "DatabaseLock.h"
#include "DatabaseSnapshot.h"
#include "Responses.h"
#include "Task.h"

Response dispatch_command_safe(const std::string &cmd_str, Task *task,
                               const DatabaseSnapshot *snap);

std::vector<DatabaseLock> dispatch_locks(const Command &cmd,
                                         const DatabaseSnapshot *snap);
