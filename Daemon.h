#pragma once

#include <string>

#include "DatabaseSnapshot.h"
#include "Responses.h"
#include "Task.h"

Response dispatch_command_safe(const std::string &cmd_str, Task *task, const DatabaseSnapshot *snap);
