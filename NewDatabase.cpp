#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <stack>
#include <utility>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "Command.h"
#include "Database.h"
#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "QueryParser.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("    %s [database]\n", argv[0]);
        return 1;
    }

    // TODO() error handling
    Database::create(argv[1]);
    return 0;
}
