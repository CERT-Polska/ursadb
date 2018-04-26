#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
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
    db->index_path(cmd.get_index_types(), path);

    return "OK";
}

std::string execute_command(const CompactCommand &cmd, Database *db) {
    db->compact();

    return "OK";
}

std::string dispatch_command(const Command &cmd, Database *db) {
    return std::visit([db](const auto &cmd) { return execute_command(cmd, db); }, cmd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("    %s database-file [bind-address]\n", argv[0]);
        return 1;
    }

    Database db(argv[1]);
    std::string bind_address = "tcp://*:9281";

    if (argc > 3) {
        std::cout << "Too many command line arguments." << std::endl;
    } else if (argc == 3) {
        bind_address = std::string(argv[2]);
    }

    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REP);
    socket.bind(bind_address);

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

    return 0;
}
