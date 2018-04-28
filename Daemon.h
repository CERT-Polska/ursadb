#pragma once

std::string dispatch_command_safe(const std::string &cmd_str, Task *task, DatabaseSnapshot *snap);
