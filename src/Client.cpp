#include <iostream>
#include <thread>
#include <zmq.hpp>

#include "libursa/Json.h"
#include "libursa/ZHelpers.h"
#include "spdlog/spdlog.h"

#include "Client.h"

static void wait_sec() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

void UrsaClient::status_worker() {
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.setsockopt(ZMQ_LINGER, 0);
    socket.setsockopt(ZMQ_RCVTIMEO, 1000);
    socket.connect(this->server_addr);

    while (!this->terminated) {
        if (!this->command_active) {
            wait_sec();
            continue;
        }

        s_send(socket, "status;");
        std::string res_str;
        uint64_t retries = 0;

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

        for (const auto &task : res_tasks) {
            if (task["connection_id"] != this->connection_id) {
                continue;
            }

            uint64_t work_done = task["work_done"];
            uint64_t work_estimated = task["work_estimated"];
            uint64_t work_perc =
                work_estimated > 0 ? work_done * 100 / work_estimated : 0;
            spdlog::info("Working... {}% ({} / {})", work_perc, work_done,
                         work_estimated);
        }

        wait_sec();
    }
}

void UrsaClient::init_conn(zmq::socket_t &socket) {
    s_send(socket, "ping;");
    auto res_str = s_recv(socket);

    if (res_str.empty()) {
        throw std::runtime_error("Failed to connect to the database!");
    }

    auto res = json::parse(res_str)["result"];
    std::string server_status = res["status"].get<std::string>();

    if (server_status != "ok") {
        std::string msg = "Server returned bad status: " + server_status;
        throw std::runtime_error(msg);
    }

    this->server_version = res["ursadb_version"];
    this->connection_id = res["connection_id"];

    spdlog::info("Connected to UrsaDB v{} (connection id: {})", server_version,
                 connection_id);
}

void UrsaClient::recv_res(zmq::socket_t &socket) {
    // enable periodic progress check
    // which is done by status worker
    this->command_active = true;

    while (this->command_active) {
        auto res_str = s_recv(socket);

        if (res_str.empty()) {
            continue;
        }

        this->command_active = false;
        auto res = json::parse(res_str);

        if (res["type"] == "error") {
            spdlog::error(res["error"]["message"].get<std::string>());
        } else {
            spdlog::info(res.dump(4));
        }
    }
}

static void s_send_cmd(zmq::socket_t &socket, std::string cmd) {
    // for user convenience
    if (cmd.back() != ';') {
        cmd = cmd + ";";
    }

    s_send(socket, cmd);
}

int UrsaClient::start() {
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.setsockopt(ZMQ_LINGER, 0);
    socket.setsockopt(ZMQ_RCVTIMEO, 1000);
    socket.connect(server_addr);

    try {
        init_conn(socket);
    } catch (std::runtime_error &e) {
        spdlog::error(e.what());
        return 1;
    }

    std::thread status_th(&UrsaClient::status_worker, this);

    while (!this->terminated) {
        if (!db_command.empty()) {
            // execute single command and exit
            s_send_cmd(socket, db_command);
        } else {
            // interactive mode
            std::cout << "ursadb> ";

            std::string cmd;
            std::getline(std::cin, cmd);

            s_send_cmd(socket, cmd);
        }

        recv_res(socket);

        if (!db_command.empty()) {
            // single command mode; exit after processing is done
            this->terminated = true;
        }
    }

    // wait for status thread to terminate
    status_th.join();

    return 0;
}

UrsaClient::UrsaClient(std::string server_addr, std::string db_command)
    : server_addr(server_addr), db_command(db_command) {}

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

    UrsaClient client(server_addr, db_command);
    return client.start();
}
