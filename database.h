#pragma once

#include "dataset.h"
#include "query.h"

class Database {
    std::vector<OnDiskDataset> datasets;
    void compact();

public:
    void query(const Query &query);
};
