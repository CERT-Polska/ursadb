#include "Client.h"

#include <editline/readline.h>
#include <unistd.h>

#include <iostream>
#include <thread>
#include <zmq.hpp>

#include "libursa/Json.h"
#include "libursa/ZHelpers.h"
#include "spdlog/spdlog.h"

bool UrsaClient::wait_sec() {
    // make small sleeps so the thread could
    // terminate quickly if it's requested to do so
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    wait_time += 10;
    return wait_time >= 1000;
}

void UrsaClient::status_worker() {
    if (!this->is_interactive) {
        return;
    }

    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.setsockopt(ZMQ_LINGER, 0);
    socket.setsockopt(ZMQ_RCVTIMEO, 1000);
    socket.connect(this->server_addr);

    while (!this->terminated) {
        if (!this->command_active || !wait_sec()) {
            continue;
        }

        wait_time = 0;
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

    if (this->is_interactive) {
        spdlog::info("Connected to UrsaDB v{} (connection id: {})",
                     server_version, connection_id);
    }
}

void UrsaClient::recv_res(zmq::socket_t &socket) {
    // enable periodic progress check
    // which is done by status worker
    this->command_active = true;
    this->wait_time = 0;

    while (this->command_active) {
        auto res_str = s_recv(socket);

        if (res_str.empty()) {
            continue;
        }

        this->command_active = false;
        auto res = json::parse(res_str);

        if (this->raw_json) {
            std::cout << res.dump(4) << std::endl;
            return;
        }

        if (res["type"] == "select") {
            for (const auto &file : res["result"]["files"]) {
                std::cout << file.get<std::string>() << std::endl;
            }
        } else if (res["type"] == "error") {
            spdlog::error(res["error"]["message"].get<std::string>());
        } else {
            std::cout << res.dump(4) << std::endl;
        }
    }
}

std::optional<std::string> UrsaClient::read_line(const std::string &prompt) {
    char *cmd_ptr{readline(prompt.c_str())};
    if (cmd_ptr == nullptr) {
        return std::nullopt;
    }
    std::string cmd{cmd_ptr};

    free(cmd_ptr);

    return cmd;
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

    std::string prompt{is_interactive ? "ursadb>" : ""};
    while (!this->terminated) {
        if (!db_command.empty()) {
            // execute single command and exit
            s_send_cmd(socket, db_command);
        } else {
            auto maybe_cmd{read_line(prompt)};
            if (!maybe_cmd) {
                this->terminated = true;
                std::cout << std::endl;
                continue;
            }

            s_send_cmd(socket, maybe_cmd.value());
        }

        recv_res(socket);

        if (!db_command.empty()) {
            // single command mode; exit after processing is done
            this->terminated = true;
        }
    }

    status_th.join();

    return 0;
}

UrsaClient::UrsaClient(std::string server_addr, std::string db_command,
                       bool is_interactive, bool raw_json)
    : server_addr(server_addr),
      db_command(db_command),
      is_interactive(is_interactive),
      raw_json(raw_json) {}

static void print_usage(const char *arg0) {
    spdlog::info("Usage: {} [server_addr] [args...]", arg0);
    spdlog::info(
        "    [server_addr]      server connection string, default: "
        "tcp://localhost:9281");
    spdlog::info(
        "    [-c <db_command>]  specific command to be run in the database, "
        "if not provided - interactive mode");
    spdlog::info(
        "    [-q]               silent mode, dump only command output");
    spdlog::info("    [-j]               force JSON output everywhere");
}

int main(int argc, char *argv[]) {
    std::string server_addr = "tcp://localhost:9281";
    std::string db_command = "";
    bool is_interactive = isatty(0);
    bool raw_json = false;

    int c;

    while ((c = getopt(argc, argv, "hqjc:")) != -1) {
        switch (c) {
            case 'q':
                is_interactive = false;
                break;
            case 'c':
                db_command = optarg;
                break;
            case 'j':
                raw_json = true;
                break;
            case 'h':
                print_usage(argc >= 1 ? argv[0] : "ursacli");
                return 0;
            default:
                print_usage(argc >= 1 ? argv[0] : "ursacli");
                spdlog::error("Failed to parse command line.");
                return 1;
        }
    }

    if (argc - optind > 1) {
        spdlog::error("Too many positional arguments provided.");
        print_usage(argc >= 1 ? argv[0] : "ursacli");
        return 1;
    } else if (argc - optind == 1) {
        server_addr = argv[optind];
    }

    try {
        UrsaClient client(server_addr, db_command, is_interactive, raw_json);
        client.start();
    } catch (const zmq::error_t &ex) {
        spdlog::error("ZeroMQ error: {}", ex.what());
        return 1;
    }

    return 0;
}
