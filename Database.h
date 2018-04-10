#pragma once

#include "OnDiskDataset.h"

class Database {
private:
    std::vector<OnDiskDataset> datasets;
    void compact();
};
