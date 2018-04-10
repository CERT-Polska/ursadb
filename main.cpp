#include <iostream>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <array>
#include <list>


#include "OnDiskDataset.h"
#include "DatasetBuilder.h"


int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage:\n");
        printf("%s index [database] [file]\n", argv[0]);
        printf("%s query [database] [query]\n", argv[0]);
        return 1;
    }

    std::string dbpath = argv[2];
    if (argv[1] == std::string("index")) {
        DatasetBuilder builder;
        for (int i = 3; i < argc; i++) {
            builder.index(argv[i]);
        }
        builder.save(dbpath);
    } else if (argv[1] == std::string("query")) {
        std::string query = argv[3];
        if (query.length() != 3) {
            printf("Sorry, I'm just a simple PoC - query.length() == 3, please.");
            return 2;
        }
        TriGram raw_query = (query[0] << 16) + (query[1] << 8) + (query[2] << 0);
        std::cout << "searching for trigram " << std::hex << raw_query << std::endl;
        OnDiskDataset *dataset = new OnDiskDataset(dbpath);
        std::vector<std::string> result = dataset->query_primitive(raw_query);

        for (auto file : result) {
            std::cout << file << std::endl;
        }
    }
    return 0;
}
