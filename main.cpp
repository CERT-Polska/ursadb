#include <iostream>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <array>
#include <list>


#include "OnDiskDataset.h"
#include "DatasetBuilder.h"
#include "Query.h"
#include "Database.h"


int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage:\n");
        printf("%s index [database] [file]\n", argv[0]);
        printf("%s query [database] [query]\n", argv[0]);
        return 1;
    }

    std::string dbpath = argv[2];
    if (argv[1] == std::string("index")) {
        Database db(dbpath);
        DatasetBuilder builder;
        for (int i = 3; i < argc; i++) {
            builder.index(argv[i]);
        }
        db.add_dataset(builder);
        db.save();
    } else if (argv[1] == std::string("select_poc")) {
        Query test = q_or({ q("more problem"), q("more problem"), q("more problem"), q("wtf") });
        test.print_query();
        Database db("db.ursa");
        std::vector<std::string> out;
        db.execute(test, out);

        std::cout << "---" << std::endl;
        for (std::string &s : out) {
            std::cout << s << std::endl;
        }
        std::cout << "---" << std::endl;
    }
    return 0;
}
