#include <iostream>
#include <thread>

#include <zmq.hpp>

#include "libursa/Json.h"
#include "libursa/ZHelpers.h"
#include "spdlog/spdlog.h"

void status_worker(std::string server_addr, std::string conn_id) {
    while (true) {
        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_REQ);
        socket.setsockopt(ZMQ_LINGER, 0);
        socket.setsockopt(ZMQ_RCVTIMEO, 1000);
        socket.connect(server_addr);

        s_send(socket, "status;");
        std::string res_str;
        unsigned int retries = 0;

        while (res_str.empty()) {
            if (retries == 30) {
                spdlog::warn(
                    "UrsaDB server seems to be unresponsive. Failed to obtain "
                    "progress for more than 30 seconds.");
            }

            res_str = s_recv(socket);
            retries++;
        }

        auto res = json::parse(res_str);
        auto res_tasks = res["result"]["tasks"];

        for (json::iterator it = res_tasks.begin(); it != res_tasks.end();
             ++it) {
            if ((*it)["connection_id"] == conn_id) {
                unsigned int work_done = (*it)["work_done"].get<unsigned int>();
                unsigned int work_estimated =
                    (*it)["work_estimated"].get<unsigned int>();
                unsigned int work_perc =
                    work_estimated > 0 ? work_done * 100 / work_estimated : 0;
                spdlog::info("Working... {}% ({} / {})", work_perc, work_done,
                             work_estimated);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

int main(int argc, char *argv[]) {
    std::string server_addr = "tcp://localhost:9281";
    std::string db_command = "";

    if (argc >= 2) {
        server_addr = std::string(argv[1]);

        if (server_addr == "-h" || server_addr == "--help") {
            spdlog::info("Usage: {} [server_addr] [db_command]", argv[0]);
            spdlog::info(
                "    server_addr - server connection string, default: "
                "tcp://localhost:9281");
            spdlog::info(
                "    db_command - specific command to be run in the database, "
                "if not provided - interactive mode");
            return 0;
        }
    }

    if (argc >= 3) {
        db_command = std::string(argv[2]);
    }

    if (argc >= 4) {
        spdlog::error("Too many arguments provided in command line.");
        return 1;
    }

    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.setsockopt(ZMQ_LINGER, 0);
    socket.setsockopt(ZMQ_RCVTIMEO, 1000);
    socket.connect(server_addr);

    s_send(socket, "ping;");
    auto res_str = s_recv(socket);

    if (res_str.empty()) {
        spdlog::error("Failed to connect to the database!");
        return 1;
    }

    auto res = json::parse(res_str)["result"];

    std::string server_version = res["ursadb_version"].get<std::string>();
    std::string server_status = res["status"].get<std::string>();
    std::string connection_id = res["connection_id"].get<std::string>();

    if (server_status != "ok") {
        spdlog::error("Server returned bad status: {}", server_status);
        return 1;
    }

    spdlog::info("Connected to UrsaDB v{} (connection id: {})", server_version,
                 connection_id);

    std::thread status_th(status_worker, server_addr, connection_id);

    while (true) {
        if (!db_command.empty()) {
            // execute single command and exit
            s_send(socket, db_command);
        } else {
            // interactive mode
            std::cout << "ursadb> ";

            std::string cmd;
            std::getline(std::cin, cmd);

            s_send(socket, cmd);
        }

        do {
            auto res_str = s_recv(socket);

            if (res_str.empty()) {
                continue;
            }

            auto res = json::parse(res_str);
            auto res_type = res["type"].get<std::string>();

            if (res_type == "error") {
                spdlog::error(res["error"]["message"].get<std::string>());
            } else {
                spdlog::info(res.dump(4));
            }

            break;
        } while (1);

        if (!db_command.empty()) {
            break;
        }
    }

    return 0;
}
