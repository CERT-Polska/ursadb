#pragma once

#include "dataset.h"
#include "query.h"

class Database {
public:
    void query(const Query &query);

private:
    std::vector<OnDiskDataset> datasets;
    void compact();
};
