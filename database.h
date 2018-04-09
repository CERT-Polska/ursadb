#include "dataset.h"
#include "query.h"

class Database {
    std::vector<OnDiskDataset> datasets;
    void compact();

public:
    void query(const Query &query);
    void index_path(const std::string &path);
};
