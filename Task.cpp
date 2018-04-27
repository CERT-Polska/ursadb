#include "Task.h"

std::string db_change_to_string(DbChangeType change) {
  switch (change) {
    case DbChangeType::Insert: return "INSERT";
    case DbChangeType::Drop: return "DROP";
  }
  return "<invalid?>";
}