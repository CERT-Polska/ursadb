#pragma once

#include <string>

#include "libursa/DatabaseSnapshot.h"
#include "libursa/Responses.h"
#include "libursa/Task.h"

Response dispatch_command_safe(const std::string &cmd_str, Task *task,
                               const DatabaseSnapshot *snap);
