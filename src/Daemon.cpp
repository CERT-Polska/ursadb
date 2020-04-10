#include <pthread.h>
#include <sys/types.h>

#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <queue>
#include <sstream>
#include <stack>
#include <utility>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "libursa/Command.h"
#include "libursa/Database.h"
#include "libursa/DatasetBuilder.h"
#include "libursa/OnDiskDataset.h"
#include "libursa/QueryParser.h"
#include "libursa/Responses.h"
#include "libursa/ResultWriter.h"
#include "NetworkService.h"

Response execute_command(const SelectCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    std::stringstream ss;
    const Query &query = cmd.get_query();

    if (cmd.iterator_requested()) {
        DatabaseName data_filename = snap->allocate_name("iterator");
        FileResultWriter writer(data_filename.get_full_path());
        snap->execute(query, cmd.get_taints(), task, &writer);
        // TODO DbChange should use DatabaseName type instead.
        DatabaseName meta_filename =
            snap->derive_name(data_filename, "itermeta");
        OnDiskIterator::construct(meta_filename, data_filename,
                                  writer.get_file_count());
        task->changes.emplace_back(DbChangeType::NewIterator,
                                   meta_filename.get_filename());
        return Response::select_iterator(meta_filename.get_id(),
                                         writer.get_file_count());
    } else {
        InMemoryResultWriter writer;
        snap->execute(query, cmd.get_taints(), task, &writer);
        return Response::select(writer.get());
    }
}

Response execute_command(const IteratorPopCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    std::vector<std::string> out;
    uint64_t iterator_position;
    uint64_t total_files;
    bool success =
        snap->read_iterator(task, cmd.get_iterator_id(), cmd.elements_to_pop(),
                            &out, &iterator_position, &total_files);

    if (success) {
        return Response::select_from_iterator(out, iterator_position,
                                              total_files);
    } else {
        return Response::error("iterator locked, try again later", true);
    }
}

Response execute_command(const IndexFromCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
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

Response execute_command(const IndexCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    snap->index_path(task, cmd.get_index_types(), cmd.get_paths());

    return Response::ok();
}

Response execute_command(const ReindexCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    const std::string &dataset_name = cmd.get_dataset_name();
    snap->reindex_dataset(task, cmd.get_index_types(), dataset_name);

    return Response::ok();
}

Response execute_command(const CompactCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    if (cmd.get_type() == CompactType::All) {
        snap->compact(task);
    } else if (cmd.get_type() == CompactType::Smart) {
        snap->smart_compact(task);
    } else {
        throw std::runtime_error("unhandled CompactType");
    }

    return Response::ok();
}

Response execute_command(const StatusCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
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

Response execute_command(const TopologyCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    std::stringstream ss;
    const std::vector<const OnDiskDataset *> &datasets = snap->get_datasets();

    std::vector<DatasetEntry> result;
    for (const auto *dataset : datasets) {
        DatasetEntry dataset_entry{/*.id:*/ dataset->get_id(),
                                   /*.size:*/ 0,
                                   /*.file_count:*/ dataset->get_file_count(),
                                   /*.taints:*/ dataset->get_taints()};

        for (const auto &index : dataset->get_indexes()) {
            IndexEntry index_entry{/*.type:*/ index.index_type(),
                                   /*.size:*/ index.real_size()};
            dataset_entry.indexes.push_back(index_entry);
            dataset_entry.size += index_entry.size;
        }

        result.push_back(dataset_entry);
    }

    return Response::topology(result);
}

Response execute_command(const PingCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    return Response::ping(bin_str_to_hex(task->conn_id));
}

Response execute_command(const TaintCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    const OnDiskDataset *ds = snap->find_dataset(cmd.get_dataset());
    if (!ds) {
        throw std::runtime_error("can't taint non-existent dataset");
    }
    const std::string &taint = cmd.get_taint();
    bool has_taint = ds->get_taints().count(taint) > 0;
    bool should_have_taint = cmd.get_mode() == TaintMode::Add;

    if (has_taint != should_have_taint) {
        task->changes.emplace_back(DbChangeType::ToggleTaint, cmd.get_dataset(),
                                   taint);
    }

    return Response::ok();
}

Response dispatch_command(const Command &cmd, Task *task,
                          const DatabaseSnapshot *snap) {
    return std::visit(
        [snap, task](const auto &cmd) {
            return execute_command(cmd, task, snap);
        },
        cmd);
}

Response dispatch_command_safe(const std::string &cmd_str, Task *task,
                               const DatabaseSnapshot *snap) {
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
    } catch (const std::runtime_error &ex) {
        std::cout << "Runtime error: " << ex.what() << std::endl;
    }
}
