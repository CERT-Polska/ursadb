#include <iostream>
#include <utility>
#include <vector>
#include <fstream>
#include <array>
#include <list>
#include <stack>

#include "OnDiskDataset.h"
#include "DatasetBuilder.h"
#include "Database.h"
#include "QueryParser.h"


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("    %s index [database] [file]\n", argv[0]);
        printf("    %s query [database] [query]\n", argv[0]);
        printf("    %s compact [database]\n", argv[0]);
        fflush(stdout);
        return 1;
    }

    std::string dbpath = argv[2];
    if (argv[1] == std::string("index")) {
        Database db(dbpath);
        DatasetBuilder builder;

        if (argc <= 3) {
            std::cout << "nothing to index" << std::endl;
            return 1;
        }

        for (int i = 3; i < argc; i++) {
            builder.index(argv[i]);
        }
        db.add_dataset(builder);
        db.save();
    } else if (argv[1] == std::string("query")) {
        if (argc != 4) {
            std::cout << "too few or too many arguments" << std::endl;
            return 1;
        }

        Query test = parse_query(argv[3]);
        std::cout << test << std::endl;
        Database db(argv[2]);
        std::vector<std::string> out;
        db.execute(test, out);
        for (std::string &s : out) {
            std::cout << s << std::endl;
        }
    } else if (argv[1] == std::string("compact")) {
        if (argc != 3) {
            std::cout << "too few or too many arguments" << std::endl;
            return 1;
        }

        Database db(argv[2]);
        db.compact();
    }
    return 0;
}
