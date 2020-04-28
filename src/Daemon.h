#pragma once

#include <string>
#include <vector>

#include "libursa/Command.h"
#include "libursa/DatabaseLock.h"
#include "libursa/DatabaseSnapshot.h"
#include "libursa/Responses.h"
#include "libursa/Task.h"

Response dispatch_command_safe(const std::string &cmd_str, Task *task,
                               const DatabaseSnapshot *snap);

std::vector<DatabaseLock> dispatch_locks(const Command &cmd,
                                         const DatabaseSnapshot *snap);
