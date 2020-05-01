#include "Daemon.h"

#include <sys/types.h>

#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <queue>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "NetworkService.h"
#include "libursa/Command.h"
#include "libursa/Database.h"
#include "libursa/DatabaseUpgrader.h"
#include "libursa/DatasetBuilder.h"
#include "libursa/FeatureFlags.h"
#include "libursa/OnDiskDataset.h"
#include "libursa/QueryParser.h"
#include "libursa/Responses.h"
#include "libursa/ResultWriter.h"
#include "libursa/Utils.h"
#include "spdlog/spdlog.h"

Response execute_command(const SelectCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    if (cmd.iterator_requested()) {
        DatabaseName data_filename = snap->allocate_name("iterator");
        FileResultWriter writer(data_filename.get_full_path());

        auto stats = snap->execute(cmd.get_query(), cmd.taints(),
                                   cmd.datasets(), task, &writer);

        // TODO DbChange should use DatabaseName type instead.
        DatabaseName meta_filename =
            snap->derive_name(data_filename, "itermeta");
        OnDiskIterator::construct(meta_filename, data_filename,
                                  writer.get_file_count());
        task->change(
            DBChange(DbChangeType::NewIterator, meta_filename.get_filename()));
        return Response::select_iterator(
            meta_filename.get_id(), writer.get_file_count(), stats.counters());
    } else {
        InMemoryResultWriter writer;
        auto stats = snap->execute(cmd.get_query(), cmd.taints(),
                                   cmd.datasets(), task, &writer);
        return Response::select(writer.get(), stats.counters());
    }
}

Response execute_command(const IteratorPopCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    std::vector<std::string> out;
    uint64_t iterator_position;
    uint64_t total_files;
    snap->read_iterator(task, cmd.get_iterator_id(), cmd.elements_to_pop(),
                        &out, &iterator_position, &total_files);

    return Response::select_from_iterator(out, iterator_position, total_files);
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

    if (cmd.ensure_unique()) {
        snap->index_files(task, cmd.get_index_types(), paths);
    } else {
        snap->force_index_files(task, cmd.get_index_types(), paths);
    }

    return Response::ok();
}

Response execute_command(const IndexCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    if (cmd.ensure_unique()) {
        snap->recursive_index_paths(task, cmd.get_index_types(),
                                    cmd.get_paths());
    } else {
        snap->force_recursive_index_paths(task, cmd.get_index_types(),
                                          cmd.get_paths());
    }

    return Response::ok();
}

Response execute_command(const ConfigGetCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    if (cmd.keys().empty()) {
        return Response::config(snap->get_config().get_all());
    }
    std::unordered_map<std::string, uint64_t> vals;
    for (const auto &key : cmd.keys()) {
        vals[key] = snap->get_config().get(ConfigKey(key));
    }
    return Response::config(vals);
}

Response execute_command(const ConfigSetCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    task->change(DBChange(DbChangeType::ConfigChange, cmd.key(),
                          std::to_string(cmd.value())));
    return Response::ok();
}

Response execute_command(const ReindexCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    const std::string &dataset_id = cmd.dataset_id();
    snap->reindex_dataset(task, cmd.get_index_types(), dataset_id);

    return Response::ok();
}

Response execute_command(const CompactCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    snap->compact_locked_datasets(task);
    return Response::ok();
}

Response execute_command(const StatusCommand &cmd, Task *task,
                         const DatabaseSnapshot *snap) {
    return Response::status(snap->get_tasks());
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
    return Response::ping(task->spec().hex_conn_id());
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
        task->change(
            DBChange(DbChangeType::ToggleTaint, cmd.get_dataset(), taint));
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
        spdlog::error("Task {} failed: {}", task->spec().id(), e.what());
        return Response::error(e.what());
    }
}

std::vector<DatabaseLock> acquire_locks(const IteratorPopCommand &cmd,
                                        const DatabaseSnapshot *snap) {
    return {IteratorLock(cmd.get_iterator_id())};
}

std::vector<DatabaseLock> acquire_locks(const ReindexCommand &cmd,
                                        const DatabaseSnapshot *snap) {
    return {DatasetLock(cmd.dataset_id())};
}

std::vector<DatabaseLock> acquire_locks(const CompactCommand &cmd,
                                        const DatabaseSnapshot *snap) {
    std::vector<std::string> to_lock;
    if (cmd.get_type() == CompactType::Smart) {
        to_lock = snap->compact_smart_candidates();
    } else {
        to_lock = snap->compact_full_candidates();
    }

    std::vector<DatabaseLock> locks;
    locks.reserve(to_lock.size());
    for (const auto &dsid : to_lock) {
        locks.emplace_back(DatasetLock(dsid));
    }

    return locks;
}

std::vector<DatabaseLock> acquire_locks(const TaintCommand &cmd,
                                        const DatabaseSnapshot *snap) {
    return {DatasetLock(cmd.get_dataset())};
}

template <typename T>
std::vector<DatabaseLock> acquire_locks(const T &anything,
                                        const DatabaseSnapshot *snap) {
    return {};
}

std::vector<DatabaseLock> dispatch_locks(const Command &cmd,
                                         const DatabaseSnapshot *snap) {
    return std::move(std::visit(
        [snap](const auto &cmd) { return acquire_locks(cmd, snap); }, cmd));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("    %s database-file [bind-address]\n", argv[0]);
        return 1;
    }

    migrate_version(argv[1]);

    try {
        Database db(argv[1]);
        spdlog::info("UrsaDB v{}", get_version_string());
        std::string bind_address = "tcp://127.0.0.1:9281";

        if (argc > 3) {
            spdlog::error("Too many command line arguments.");
        } else if (argc == 3) {
            bind_address = std::string(argv[2]);
        }

        NetworkService service(db, bind_address);
        service.run();
    } catch (const std::runtime_error &ex) {
        spdlog::error("Runtime error: {}", ex.what());
        return 1;
    } catch (const json::exception &ex) {
        spdlog::error("JSON error: {}", ex.what());
        return 1;
    } catch (const zmq::error_t &ex) {
        spdlog::error("ZeroMQ error: {}", ex.what());
        return 1;
    }

    return 0;
}
