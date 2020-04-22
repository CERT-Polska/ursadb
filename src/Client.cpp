#include "Client.h"

#include <unistd.h>

#include <iostream>
#include <thread>
#include <zmq.hpp>

#include "libursa/Json.h"
#include "libursa/ZHelpers.h"
#include "spdlog/spdlog.h"

void UrsaClient::check_task_status(const std::string &conn_id) {
    s_send<std::string_view>(&status_socket, "status;");

    std::string res_str;
    constexpr int max_retries = 30;
    for (int i = 0; i < max_retries && res_str.empty(); i++) {
        try {
            res_str = s_recv<std::string>(&status_socket);
        } catch (const std::runtime_error &) {
        }
    }

    if (res_str.empty()) {
        spdlog::warn(
            "UrsaDB server seems to be unresponsive. Failed to obtain "
            "progress for more than 30 seconds.");
        return;
    }

    auto res = json::parse(res_str);
    auto res_tasks = res["result"]["tasks"];

    for (const auto &task : res_tasks) {
        if (task["connection_id"] != conn_id) {
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

void UrsaClient::check_conn_status(zmq::socket_t *socket) {
    s_send<std::string_view>(socket, "ping;");
    auto res_str = s_recv<std::string>(socket);

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

void UrsaClient::recv_res(zmq::socket_t *socket) {
    std::string result;
    while (result.empty()) {
        try {
            result = s_recv<std::string>(socket);
        } catch (const std::runtime_error &ex) {
            check_task_status(this->connection_id);
        }
    }

    auto res = json::parse(result);
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

static void s_send_cmd(zmq::socket_t *socket, std::string cmd) {
    // for user convenience
    if (cmd.back() != ';') {
        cmd = cmd + ";";
    }
    s_send(socket, cmd);
}

void UrsaClient::setup_connection() {
    auto make_socket = [this]() {
        zmq::socket_t socket(context, ZMQ_REQ);
        socket.setsockopt(ZMQ_LINGER, 0);
        socket.setsockopt(ZMQ_RCVTIMEO, 1000);
        socket.connect(server_addr);
        return socket;
    };

    spdlog::info("Connecting to {}", server_addr);
    cmd_socket = make_socket();
    check_conn_status(&cmd_socket);
    status_socket = make_socket();
}

void UrsaClient::one_shot_command(const std::string &cmd) {
    setup_connection();
    s_send_cmd(&cmd_socket, cmd);
    recv_res(&cmd_socket);
}

void UrsaClient::start() {
    setup_connection();

    for (;;) {
        if (is_interactive) {
            std::cout << "ursadb> ";
        }

        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd.empty()) {
            break;
        }

        s_send_cmd(&cmd_socket, cmd);
        recv_res(&cmd_socket);
    }
}

UrsaClient::UrsaClient(std::string server_addr, bool is_interactive,
                       bool raw_json)
    : server_addr(server_addr),
      is_interactive(is_interactive),
      raw_json(raw_json),
      context(1),
      // deafault constructor workaround
      cmd_socket(context, ZMQ_REQ),
      status_socket(context, ZMQ_REQ) {}

void print_usage(std::string_view exec_name) {
    // clang-format off
    fmt::print(stderr, "Usage: {} [option] [server_addr]", exec_name);
    fmt::print(stderr, "    [server_addr]      server connection string, default: tcp://localhost:9281");
    fmt::print(stderr, "    [-c <db_command>]  specific command to be run in the database, if not provided - interactive mode");
    fmt::print(stderr, "    [-q]               silent mode, dump only command output");
    fmt::print(stderr, "    [-j]               force JSON output everywhere");
    // clang-format on
}

int main(int argc, char *argv[]) {
    std::string server_addr = "tcp://localhost:9281";
    std::string db_command = "";
    bool is_interactive = isatty(STDIN_FILENO);
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
        UrsaClient client(server_addr, is_interactive, raw_json);
        if (!db_command.empty()) {
            client.one_shot_command(db_command);
        } else {
            client.start();
        }
    } catch (const std::runtime_error &ex) {
        spdlog::error("Runtime error: {}", ex.what());
        return 1;
    } catch (const zmq::error_t &ex) {
        spdlog::error("ZeroMQ error: {}", ex.what());
        return 1;
    }
}
