#include "Client.h"

#include <unistd.h>

#include <iostream>
#include <thread>
#include <utility>
#include <zmq.hpp>

#include "libursa/Json.h"
#include "libursa/ZHelpers.h"
#include "spdlog/spdlog.h"

void UrsaClient::check_task_status(const std::string &conn_id) {
    s_send<std::string_view>(&status_socket, "status;", ZMQTRACE);

    std::string res_str;
    while (res_str.empty()) {
        auto response{s_try_recv<std::string>(&status_socket)};
        if (response.has_value()) {
            res_str = *response;
        }
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
    s_send<std::string_view>(socket, "ping;", ZMQTRACE);
    auto res_str = s_recv<std::string>(socket, ZMQTRACE);

    if (res_str.empty()) {
        throw std::runtime_error("Failed to connect to the database!");
    }

    auto res = json::parse(res_str)["result"];
    auto server_status = res["status"].get<std::string>();

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
        auto r{s_try_recv<std::string>(socket)};
        if (r.has_value()) {
            result = *r;
        } else {
            check_task_status(connection_id);
        }
    }

    auto res = json::parse(result);
    if (this->raw_json) {
        std::cout << res.dump(4) << std::endl;
        return;
    }

    if (res["type"] == "select" && res.find("iterator") == res.end()) {
        for (const auto &file : res["result"]["files"]) {
            std::cout << file.get<std::string>() << std::endl;
        }
    } else if (res["type"] == "topology") {
        for (const auto &item : res["result"]["datasets"].items()) {
            std::cout << "dataset " << item.key() << " ";
            std::cout << "[" << std::setw(10)
                      << item.value()["file_count"].get<int>() << "]";
            for (const auto &ndx : item.value()["indexes"].items()) {
                std::cout << " (" << ndx.value()["type"].get<std::string>()
                          << ")";
            }
            std::cout << std::endl;
        }
    } else if (res["type"] == "status") {
        for (const auto &item : res["result"]["tasks"].items()) {
            const auto &task = item.value();
            uint64_t estimated = task["work_estimated"];
            uint64_t done = task["work_done"];
            uint64_t id = task["id"];
            int done_per_10 = 0;
            if (estimated != 0) {
                done_per_10 = done * 10 / estimated;
            }
            std::cout << "task " << std::setw(9) << id;
            std::cout << " [";
            for (int i = 0; i < 10; i++) {
                std::cout << ((i < done_per_10) ? "#" : " ");
            }
            std::cout << "] ";
            std::string req = task["request"];
            if (req.size() <= 110) {
                std::cout << req;
            } else {
                std::cout << req.substr(0, 53);
                std::cout << " (...) ";
                std::cout << req.substr(req.size() - 50, 50);
            }
            std::cout << std::endl;
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
    s_send(socket, cmd, ZMQTRACE);
}

void UrsaClient::setup_connection() {
    auto make_socket = [this]() {
        zmq::socket_t socket(context, ZMQ_REQ);
        socket.setsockopt(ZMQ_LINGER, 0);
        socket.setsockopt(ZMQ_RCVTIMEO, 1000);
        socket.connect(server_addr);
        return socket;
    };

    if (is_interactive) {
        spdlog::info("Connecting to {}", server_addr);
    }
    cmd_socket = make_socket();
    check_conn_status(&cmd_socket);
    status_socket = make_socket();
}

void UrsaClient::one_shot_command(const std::string &cmd) {
    s_send_cmd(&cmd_socket, cmd);
    recv_res(&cmd_socket);
}

void UrsaClient::start() {
    for (;;) {
        if (is_interactive) {
            std::cout << "ursadb> ";
        }

        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd.empty()) {
            break;
        }

        one_shot_command(cmd);
    }
}

UrsaClient::UrsaClient(std::string server_addr, bool is_interactive,
                       bool raw_json)
    : server_addr(std::move(server_addr)),
      is_interactive(is_interactive),
      raw_json(raw_json),
      context(1),
      // deafault constructor workaround
      cmd_socket(context, ZMQ_REQ),
      status_socket(context, ZMQ_REQ) {
    setup_connection();
}

void print_usage(std::string_view exec_name) {
    // clang-format off
    fmt::print(stderr, "Usage: {} [option] [server_addr]\n", exec_name);
    fmt::print(stderr, "    [server_addr]      server connection string, default: tcp://localhost:9281\n");
    fmt::print(stderr, "    [-c <db_command>]  specific command to be run in the database, if not provided - interactive mode\n");
    fmt::print(stderr, "    [-q]               silent mode, dump only command output\n");
    fmt::print(stderr, "    [-j]               force JSON output everywhere\n");
    // clang-format on
}

int main(int argc, char *argv[]) {
    std::string server_addr = "tcp://localhost:9281";
    std::string db_command;
    bool is_interactive = isatty(STDIN_FILENO) != 0;
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
    }
    if (argc - optind == 1) {
        server_addr = argv[optind];
    }

    try {
        UrsaClient client(std::move(server_addr), is_interactive, raw_json);
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
