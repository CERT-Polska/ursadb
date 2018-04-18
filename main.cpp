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

std::string execute_command(const SelectCommand &cmd, Database *db) {
    std::stringstream ss;

    const Query &query = cmd.get_query();
    std::vector<std::string> out;
    db->execute(query, out);
    ss << "OK\n";
    for (std::string &s : out) {
        ss << s << "\n";
    }

    return ss.str();
}

std::string execute_command(const IndexCommand &cmd, Database *db) {
    const std::string &path = cmd.get_path();
    std::vector<IndexType> types = {IndexType::GRAM3, IndexType::TEXT4};
    db->index_path(types, path);

    return "OK";
}

std::string execute_command(const CompactCommand &cmd, Database *db) {
    db->compact();

    return "OK";
}

std::string dispatch_command(const Command &cmd, Database *db) {
    return std::visit([db](const auto &cmd) {
        return execute_command(cmd, db);
    }, cmd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("    %s [database]\n", argv[0]);
        printf("    %s [database] server\n", argv[0]);
        fflush(stdout);
        return 1;
    }

    Database db(argv[1]);

    if (argc == 2) {
        while (true) {
            std::string input;
            std::cout << "> ";
            std::getline(std::cin, input);
            try {
                Command cmd = parse_command(input);
                std::cout << dispatch_command(cmd, &db) << std::endl;
            } catch (std::runtime_error &e) {
                std::cout << "Command failed: " << e.what() << std::endl;
            }
        }
    } else if (argv[2] == std::string("server")) {
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
                std::string s = dispatch_command(cmd, &db);
                zmq::message_t reply(s.data(), s.size());
                socket.send(reply);
            } catch (std::runtime_error &e) {
                std::cout << "Command failed: " << e.what() << std::endl;
                std::string s = std::string("ERR ") + e.what() + "\n";
                zmq::message_t reply(s.data(), s.size());
                socket.send(reply);
            }
        }
    } else {
        std::cout << "Unknown mode: " << argv[2] << std::endl;
    }
    return 0;
}
