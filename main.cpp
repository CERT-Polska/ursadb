#include <iostream>
#include <utility>
#include <vector>
#include <fstream>
#include <array>
#include <list>
#include <stack>
#include <zmq.hpp>

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
        printf("    %s server", argv[0]);
        fflush(stdout);
        return 1;
    }

    std::string dbpath = argv[2];
    if (argv[1] == std::string("index")) {
        Database db(dbpath);


        if (argc <= 3) {
            std::cout << "nothing to index" << std::endl;
            return 1;
        }

        std::vector<IndexType> types = {IndexType::GRAM3, IndexType::TEXT4};
        for (int i = 3; i < argc; i++) {
            db.index_path(types, argv[i]);
        }
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
    } else if (argv[1] == std::string("server")) {
        zmq::context_t context (1);
        zmq::socket_t socket (context, ZMQ_REP);
        socket.bind("tcp://*:5555");

        while (true) {
            // TODO implement actual server
            zmq::message_t request;

            socket.recv(&request);
            std::cout << "Received Hello" << std::endl;

            zmq::message_t reply (5);
            memcpy (reply.data (), "World", 5);
            socket.send (reply);
        }
    }
    return 0;
}
