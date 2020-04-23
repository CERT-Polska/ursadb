#pragma once

#include <string_view>

// Historically, ursadb database files were very stable. But we don't want
// and don't have to guarantee their exact structure - especially since
// they're JSON and can be easily extended with new data. This function will
// read the db files from disk, upgrade them if necessary, and return.
void migrate_version(std::string_view path);
