#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <stack>
#include <utility>
#include <vector>
#include <variant>
#include <zmq.hpp>

#include "Database.h"
#include "Command.h"
#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "QueryParser.h"

void execute_command(const Command &cmd, Database *db) {
        if (std::holds_alternative<SelectCommand>(cmd)) {
            const Query &select = std::get<SelectCommand>(cmd).get_query();
            std::cout << select << std::endl;
            std::vector<std::string> out;
            db->execute(select, out);
            for (std::string &s : out) {
                std::cout << s << std::endl;
            }
        } else if (std::holds_alternative<IndexCommand>(cmd)) {
            const std::string &path = std::get<IndexCommand>(cmd).get_path();
            std::vector<IndexType> types = {IndexType::GRAM3, IndexType::TEXT4};
            db->index_path(types, path);
        }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("    %s command [database] [cmd]\n", argv[0]);
        printf("    %s compact [database]\n", argv[0]);
        printf("    %s server", argv[0]);
        fflush(stdout);
        return 1;
    }

    std::string dbpath = argv[2];
    if (argv[1] == std::string("index")) {
    } else if (argv[1] == std::string("command")) {
        if (argc != 4) {
            std::cout << "too few or too many arguments" << std::endl;
            return 1;
        }
        Database db(argv[2]);
        Command cmd = parse_command(argv[3]);
        execute_command(cmd, &db);
    } else if (argv[1] == std::string("compact")) {
        if (argc != 3) {
            std::cout << "too few or too many arguments" << std::endl;
            return 1;
        }

        Database db(argv[2]);
        db.compact();
    } else if (argv[1] == std::string("server")) {
        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_REP);
        socket.bind("tcp://*:5555");

        while (true) {
            // TODO implement actual server
            zmq::message_t request;

            socket.recv(&request);
            std::cout << "Received Hello" << std::endl;

            zmq::message_t reply(5);
            memcpy(reply.data(), "World", 5);
            socket.send(reply);
        }
    }
    return 0;
}
