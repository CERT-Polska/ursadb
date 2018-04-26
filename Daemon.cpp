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

std::string execute_command(const SelectCommand &cmd, Task *task, Database *db) {
    std::stringstream ss;

    const Query &query = cmd.get_query();
    std::vector<std::string> out;
    db->execute(query, task, &out);
    ss << "OK\n";
    for (std::string &s : out) {
        ss << s << "\n";
    }

    return ss.str();
}

std::string execute_command(const IndexCommand &cmd, Task *task, Database *db) {
    const std::string &path = cmd.get_path();
    db->index_path(task, cmd.get_index_types(), path);

    return "OK";
}

std::string execute_command(const CompactCommand &cmd, Task *task, Database *db) {
    db->compact(task);

    return "OK";
}

std::string execute_command(const StatusCommand &cmd, Task *task, Database *db) {
    std::stringstream ss;
    const std::vector<Task> &tasks = db->current_tasks();

    ss << "OK\n";

    for (const auto &ts : tasks) {
        ss << ts.id << ": " << ts.work_estimated << " " << ts.work_done << "\n";
    }

    return ss.str();
}

std::string dispatch_command(const Command &cmd, Task *task, Database *db) {
    return std::visit([db, task](const auto &cmd) { return execute_command(cmd, task, db); }, cmd);
}

std::string dispatch_command_safe(const std::string &cmd_str, Database *db) {
    try {
        Command cmd = parse_command(cmd_str);
        Task *task = db->allocate_task();
        return dispatch_command(cmd, task, db);
    } catch (std::runtime_error &e) {
        std::cout << "Command failed: " << e.what() << std::endl;
        return std::string("ERR ") + e.what() + "\n";
    }
}

struct WorkerArgs {
    Database *db;
    zmq::context_t *context;
};

void *worker_routine(void *arg) {
    auto *wa = (WorkerArgs *) arg;
    auto *context = wa->context;
    zmq::socket_t socket(*context, ZMQ_REP);
    socket.connect("inproc://workers");

    while (true) {
        zmq::message_t request;

        socket.recv(&request);
        std::string cmd_str = std::string(static_cast<char *>(request.data()), request.size());
        std::cout << "Received request " << cmd_str << std::endl;

        std::string s = dispatch_command_safe(cmd_str, wa->db);
        zmq::message_t reply(s.data(), s.size());
        socket.send(reply);
    }
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

    zmq::context_t context (1);
    zmq::socket_t clients (context, ZMQ_ROUTER);
    clients.bind(bind_address);
    zmq::socket_t workers (context, ZMQ_DEALER);
    workers.bind("inproc://workers");

    WorkerArgs wa = {&db, &context};

    for (int thread_nbr = 0; thread_nbr != 5; thread_nbr++) {
        pthread_t worker;
        pthread_create (&worker, NULL, worker_routine, (void *) &wa);
    }

    zmq::proxy(static_cast<void *>(clients), static_cast<void *>(workers), NULL);
    return 0;
}
