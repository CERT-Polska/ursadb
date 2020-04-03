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
#include "Responses.h"
#include "NetworkService.h"

Response execute_command(const SelectCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    std::stringstream ss;

    const Query &query = cmd.get_query();
    const std::vector<std::string> &taints = cmd.get_taints();
    for (auto taint: taints) {
        std::cout << "taint " << taint << std::endl;
    }
    std::vector<std::string> out;
    snap->execute(query, taints, task, &out);
    return Response::select(out);
}

Response execute_command(const IndexFromCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    const auto &path_list_fname = cmd.get_path_list_fname();

    std::vector<std::string> paths;
    std::ifstream inf(path_list_fname, std::ifstream::binary);

    if (!inf) {
        throw std::runtime_error("failed to open file");
    }

    inf.exceptions(std::ifstream::badbit);

    while (!inf.eof()) {
        std::string filename;
        std::getline(inf, filename);

        if (!filename.empty()) {
            paths.push_back(filename);
        }
    }

    snap->index_path(task, cmd.get_index_types(), paths);

    return Response::ok();
}

Response execute_command(const IndexCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    snap->index_path(task, cmd.get_index_types(), cmd.get_paths());

    return Response::ok();
}

Response execute_command(const ReindexCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    const std::string &dataset_name = cmd.get_dataset_name();
    snap->reindex_dataset(task, cmd.get_index_types(), dataset_name);

    return Response::ok();
}

Response execute_command(const CompactCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    if (cmd.get_type() == CompactType::All) {
        snap->compact(task);
    } else if (cmd.get_type() == CompactType::Smart) {
        snap->smart_compact(task);
    } else {
        throw std::runtime_error("unhandled CompactType");
    }

    return Response::ok();
}

Response execute_command(const StatusCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    std::stringstream ss;
    const std::map<uint64_t, Task> &tasks = snap->get_tasks();

    std::vector<TaskEntry> task_data;
    for (const auto &pair : tasks) {
        const Task &t = pair.second;
        task_data.push_back(TaskEntry{
            /*.id:*/ std::to_string(t.id),
            /*.connection_id:*/ bin_str_to_hex(t.conn_id),
            /*.request:*/ t.request_str,
            /*.work_done:*/ t.work_done,
            /*.work_estimated:*/ t.work_estimated,
            /*.epoch_ms:*/ t.epoch_ms,
        });
    }
    return Response::status(task_data);
}

Response execute_command(const TopologyCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    std::stringstream ss;
    const std::vector<const OnDiskDataset *> &datasets = snap->get_datasets();

    std::vector<DatasetEntry> result;
    for (const auto *dataset : datasets) {
        DatasetEntry dataset_entry {
            /*.id:*/ dataset->get_id(),
            /*.size:*/ 0,
            /*.file_count:*/ dataset->get_file_count(),
            /*.taints:*/ dataset->get_taints()
        };

        for (const auto &index : dataset->get_indexes()) {
            IndexEntry index_entry{
                /*.type:*/ index.index_type(),
                /*.size:*/ index.real_size()
            };
            dataset_entry.indexes.push_back(index_entry);
            dataset_entry.size += index_entry.size;
        }

        result.push_back(dataset_entry);
    }

    return Response::topology(result);
}

Response execute_command(const PingCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    return Response::ping(bin_str_to_hex(task->conn_id));
}

Response execute_command(const TaintCommand &cmd, Task *task, const DatabaseSnapshot *snap) {
    const OnDiskDataset *ds = snap->find_dataset(cmd.get_dataset());
    if (!ds) {
        throw std::runtime_error("can't taint non-existend dataset");
    }
    const std::string &taint = cmd.get_taint();
    bool has_taint = ds->get_taints().count(taint) > 0;
    bool should_have_taint = cmd.get_mode() == TaintMode::Add;

    if (has_taint != should_have_taint) {
        task->changes.emplace_back(DbChangeType::ToggleTaint, cmd.get_dataset(), taint);
    }

    return Response::ok();
}

Response dispatch_command(const Command &cmd, Task *task, const DatabaseSnapshot *snap) {
    return std::visit(
            [snap, task](const auto &cmd) { return execute_command(cmd, task, snap); }, cmd);
}

Response dispatch_command_safe(const std::string &cmd_str, Task *task, const DatabaseSnapshot *snap) {
    try {
        Command cmd = parse_command(cmd_str);
        return dispatch_command(cmd, task, snap);
    } catch (std::runtime_error &e) {
        std::cout << "Command failed: " << e.what() << std::endl;
        return Response::error(e.what());
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("    %s database-file [bind-address]\n", argv[0]);
        return 1;
    }

    try {
        Database db(argv[1]);

        std::string bind_address = "tcp://127.0.0.1:9281";

        if (argc > 3) {
            std::cout << "Too many command line arguments." << std::endl;
        } else if (argc == 3) {
            bind_address = std::string(argv[2]);
        }

        NetworkService service(db, bind_address);
        service.run();
    } catch (const std::runtime_error& ex) {
        std::cout << "Runtime error: " << ex.what() << std::endl;
    }
}
