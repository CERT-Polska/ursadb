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

std::string execute_command(const Command &cmd, Database *db) {
    std::stringstream ss;

    if (std::holds_alternative<SelectCommand>(cmd)) {
        const Query &select = std::get<SelectCommand>(cmd).get_query();
        std::cout << select << std::endl;
        std::vector<std::string> out;
        db->execute(select, out);
        ss << "OK" << std::endl;
        for (std::string &s : out) {
            ss << s << std::endl;
        }
    } else if (std::holds_alternative<IndexCommand>(cmd)) {
        const std::string &path = std::get<IndexCommand>(cmd).get_path();
        std::vector<IndexType> types = {IndexType::GRAM3, IndexType::TEXT4};
        db->index_path(types, path);
        ss << "OK" << std::endl;
    } else {
        throw std::runtime_error("Invalid command type.");
    }

    return ss.str();
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
        std::string res = execute_command(cmd, &db);
        std::cout << res << std::endl;
    } else if (argv[1] == std::string("compact")) {
        if (argc != 3) {
            std::cout << "too few or too many arguments" << std::endl;
            return 1;
        }

        Database db(argv[2]);
        db.compact();
    } else if (argv[1] == std::string("server")) {
        Database db(argv[2]);

        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_REP);
        socket.bind("tcp://*:9281");

        while (true) {
            zmq::message_t request;

            socket.recv(&request);
            std::string cmd_str = std::string(static_cast<char *>(request.data()), request.size());
            std::cout << "Received request " << cmd_str << std::endl;

            try {
                Command cmd = parse_command(cmd_str);
                std::string s = execute_command(cmd, &db);
                zmq::message_t reply(s.data(), s.size());
                socket.send(reply);
            } catch (std::runtime_error &e) {
                std::cout << "Command failed: " << e.what() << std::endl;
                std::stringstream ss;
                ss << "ERR " << e.what() << std::endl;
                std::string s = ss.str();
                zmq::message_t reply(s.data(), s.size());
                socket.send(reply);
            }
        }
    }
    return 0;
}
