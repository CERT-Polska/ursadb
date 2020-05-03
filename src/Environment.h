#pragma once
// Utility functions for setting up the environment necessary for loading
// the database.

// Increase the limit of open files.
void fix_rlimit();
