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

#include "zhelpers.h"

#include "Command.h"
#include "Database.h"
#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "QueryParser.h"

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
    const std::map<uint64_t, Task> *tasks = snap->get_tasks();

    ss << "OK\n";
    for (const auto &pair : *tasks) {
        const Task &ts = pair.second;
        ss << ts.id << ": " << ts.work_done << " " << ts.work_estimated << "\n";
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
            ss << "INDEX " << dataset->get_id() << "." << index_type << "\n";
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

struct WorkerArgs {
    int worker_nbr;
    Database *db;
    std::map<std::string, DatabaseSnapshot> *snapshots;
};

static void *worker_thread(void *arg) {
    auto *wa = static_cast<WorkerArgs *>(arg);

    zmq::context_t context(1);
    zmq::socket_t worker(context, ZMQ_REQ);

    std::string my_addr = std::to_string(wa->worker_nbr);
    worker.setsockopt(ZMQ_IDENTITY, my_addr.c_str(), my_addr.length());
    worker.connect("ipc://backend.ipc");

    //  Tell backend we're ready for work
    s_send(worker, "READY");

    for (;;) {
        //  Read and save all frames until we get an empty frame
        //  In this example there is only 1 but it could be more
        std::string address = s_recv(worker);
        assert(s_recv(worker).size() == 0);

        std::string task_id_str = s_recv(worker);
        assert(s_recv(worker).size() == 0);

        uint64_t task_id = stoul(task_id_str);
        Task &task = wa->db->current_tasks().at(task_id);

        //  Get request, send reply
        std::string request = s_recv(worker);
        DatabaseSnapshot *snap = &wa->snapshots->at(my_addr);
        std::cout << "Worker: " << request << std::endl;

        std::string s = dispatch_command_safe(request, &task, snap);

        s_sendmore(worker, address);
        s_sendmore(worker, "");
        s_send(worker, s);
    }
    return (NULL);
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

    //  Prepare our context and sockets
    zmq::context_t context(1);
    zmq::socket_t frontend(context, ZMQ_ROUTER);
    zmq::socket_t backend(context, ZMQ_ROUTER);

    frontend.bind(bind_address);
    backend.bind("ipc://backend.ipc");

    std::map<std::string, DatabaseSnapshot> snapshots;

    int worker_nbr;
    for (worker_nbr = 0; worker_nbr < 3; worker_nbr++) {
        auto *wa = new WorkerArgs{worker_nbr, &db, &snapshots};
        pthread_t worker;
        pthread_create(&worker, NULL, worker_thread, (void *)wa);
    }

    //  Logic of LRU loop
    //  - Poll backend always, frontend only if 1+ worker ready
    //  - If worker replies, queue worker as ready and forward reply
    //    to client if necessary
    //  - If client requests, pop next worker and send request to it
    //
    //  A very simple queue structure with known max size
    std::queue<std::string> worker_queue;
    std::map<std::string, uint64_t> worker_task_ids;

    for (;;) {
        //  Initialize poll set
        //  Always poll for worker activity on backend
        //  Poll front-end only if we have available workers
        zmq::pollitem_t items[] = {{static_cast<void *>(backend), 0, ZMQ_POLLIN, 0},
                                   {static_cast<void *>(frontend), 0, ZMQ_POLLIN, 0}};
        if (worker_queue.size()) {
            zmq::poll(&items[0], 2, -1);
        } else {
            zmq::poll(&items[0], 1, -1);
        }

        //  Handle worker activity on backend
        if (items[0].revents & ZMQ_POLLIN) {
            //  Queue worker address for LRU routing
            std::string worker_addr = s_recv(backend);
            uint64_t did_task = worker_task_ids[worker_addr];
            std::cout << "worker finished: " << worker_addr << ", he was doing task " << did_task
                      << std::endl;
            snapshots.erase(worker_addr);

            if (did_task != 0) {
                const auto &changes = db.current_tasks().at(did_task).changes;
                for (const auto &change : changes) {
                    if (change.first == DbChangeType::Insert) {
                        db.load_dataset(change.second);
                    } else if (change.first == DbChangeType::Drop
                    ) {
                        db.drop_dataset(change.second);
                    } else {
                        std::cout << "unknown change" << std::endl;
                    }

                    std::cout << db_change_to_string(change.first) << " " << change.second << std::endl;
                }

                if (!changes.empty()) {
                    db.save();
                }

                db.current_tasks().erase(did_task);

                // --- GC ---
                std::set<const OnDiskDataset *> required_datasets;
                for (const auto *ds : db.working_sets()) {
                    required_datasets.insert(ds);
                }

                for (const auto &p : snapshots) {
                    for (const auto *ds : p.second.get_datasets()) {
                        required_datasets.insert(ds);
                    }
                }

                std::vector<std::string> drop_list;
                for (const auto &set : db.loaded_sets()) {
                    if (required_datasets.count(set.get()) == 0) {
                        // set is loaded but not required
                        drop_list.push_back(set.get()->get_name());
                    }
                }

                for (const auto &ds : drop_list) {
                    db.unload_dataset(ds);
                }
            }

            worker_queue.push(worker_addr);

            //  Second frame is empty
            assert(s_recv(backend).size() == 0);

            //  Third frame is READY or else a client reply address
            std::string client_addr = s_recv(backend);

            //  If client reply, send rest back to frontend
            if (client_addr.compare("READY") != 0) {
                assert(s_recv(backend).size() == 0);

                std::string reply = s_recv(backend);
                s_sendmore(frontend, client_addr);
                s_sendmore(frontend, "");
                s_send(frontend, reply);
            }
        }
        if (items[1].revents & ZMQ_POLLIN) {
            //  Now get next client request, route to LRU worker
            //  Client request is [address][empty][request]
            std::string client_addr = s_recv(frontend);

            assert(s_recv(frontend).size() == 0);

            std::string request = s_recv(frontend);

            std::string worker_addr = worker_queue.front(); // worker_queue [0];
            worker_queue.pop();

            Task *task = db.allocate_task();
            snapshots.insert_or_assign(worker_addr, db.snapshot());
            worker_task_ids[worker_addr] = task->id;
            std::ostringstream ss;
            ss << task->id; // TODO passing string o_O

            s_sendmore(backend, worker_addr);
            s_sendmore(backend, "");
            s_sendmore(backend, client_addr);
            s_sendmore(backend, "");
            s_sendmore(backend, ss.str());
            s_sendmore(backend, "");
            s_send(backend, request);
        }
    }
}
