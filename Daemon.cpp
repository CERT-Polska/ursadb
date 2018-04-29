#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <pthread.h>
#include <queue>
#include <sstream>
#include <stack>
#include <sys/types.h>
#include <utility>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "Command.h"
#include "Database.h"
#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "QueryParser.h"
#include "NetworkService.h"

std::string execute_command(const SelectCommand &cmd, Task *task, DatabaseSnapshot *snap) {
    std::stringstream ss;

    const Query &query = cmd.get_query();
    std::vector<std::string> out;
    snap->execute(query, task, &out);
    ss << "OK\n";
    for (std::string &s : out) {
        ss << s << "\n";
    }

    return ss.str();
}

std::string execute_command(const IndexCommand &cmd, Task *task, DatabaseSnapshot *snap) {
    const std::string &path = cmd.get_path();
    snap->index_path(task, cmd.get_index_types(), path);

    return "OK";
}

std::string execute_command(const CompactCommand &cmd, Task *task, DatabaseSnapshot *snap) {
    snap->compact(task);

    return "OK";
}

std::string execute_command(const StatusCommand &cmd, Task *task, DatabaseSnapshot *snap) {
    std::stringstream ss;
    const std::map<uint64_t, Task> &tasks = snap->get_tasks();

    ss << "OK\n";
    for (const auto &pair : tasks) {
        ss << pair.second.id << ": " << pair.second.work_done << " " << pair.second.work_estimated << "\n";
    }
    return ss.str();
}

std::string execute_command(const TopologyCommand &cmd, Task *task, DatabaseSnapshot *snap) {
    std::stringstream ss;
    const std::vector<const OnDiskDataset *> &datasets = snap->get_datasets();

    ss << "OK\n";
    for (const auto *dataset : datasets) {
        ss << "DATASET " << dataset->get_id() << "\n";
        for (const auto &index : dataset->get_indexes()) {
            std::string index_type = get_index_type_name(index.index_type());
            ss << "INDEX " << dataset->get_id() << " " << index_type << "\n";
        }
    }

    return ss.str();
}

std::string dispatch_command(const Command &cmd, Task *task, DatabaseSnapshot *snap) {
    return std::visit(
            [snap, task](const auto &cmd) { return execute_command(cmd, task, snap); }, cmd);
}

std::string dispatch_command_safe(const std::string &cmd_str, Task *task, DatabaseSnapshot *snap) {
    try {
        Command cmd = parse_command(cmd_str);
        return dispatch_command(cmd, task, snap);
    } catch (std::runtime_error &e) {
        std::cout << "Command failed: " << e.what() << std::endl;
        return std::string("ERR ") + e.what() + "\n";
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("    %s database-file [bind-address]\n", argv[0]);
        return 1;
    }

    Database db(argv[1]);
    std::string bind_address = "tcp://127.0.0.1:9281";

    if (argc > 3) {
        std::cout << "Too many command line arguments." << std::endl;
    } else if (argc == 3) {
        bind_address = std::string(argv[2]);
    }

    NetworkService service(db, bind_address);
    service.run();
}
