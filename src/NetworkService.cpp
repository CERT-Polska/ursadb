#include "NetworkService.h"

#include <thread>

#include "Environment.h"
#include "libursa/Daemon.h"
#include "libursa/DatabaseUpgrader.h"
#include "libursa/QueryParser.h"
#include "libursa/Responses.h"
#include "libursa/ZHelpers.h"
#include "spdlog/spdlog.h"

[[noreturn]] void WorkerContext::operator()() {
    zmq::context_t context(1);
    zmq::socket_t worker(context, ZMQ_REQ);

    worker.setsockopt(ZMQ_IDENTITY, identity.c_str(), identity.length());
    worker.connect("ipc://backend.ipc");

    //  Tell backend we're ready for work
    s_send<NetAction>(&worker, NetAction::Ready, ZMQTRACE);

    for (;;) {
        //  Read and save all frames until we get an empty frame
        //  In this example there is only 1 but it could be more
        auto address{s_recv<std::string>(&worker, ZMQTRACE)};

        s_recv_padding(&worker, ZMQTRACE);

        //  Get request, send reply
        auto request{s_recv<std::string>(&worker, ZMQTRACE)};
        spdlog::info("TASK: start [{}]: {}", task->spec().id(), request);

        Response response = dispatch_command_safe(request, &*task, &snap);
        // Note: optionally add funny metadata to response here (like request
        // time, assigned worker, etc)
        std::string s = response.to_string();

        s_send<NetAction>(&worker, NetAction::Response, ZMQTRACE, ZMQ_SNDMORE);
        s_send_padding(&worker, ZMQTRACE, ZMQ_SNDMORE);
        s_send(&worker, address, ZMQTRACE, ZMQ_SNDMORE);
        s_send_padding(&worker, ZMQTRACE, ZMQ_SNDMORE);
        s_send(&worker, s, ZMQTRACE);
    }
}

void NetworkService::run() {
    uint64_t num_workers = db.config().get(ConfigKey::database_workers());
    for (uint64_t worker_no = 0; worker_no < num_workers; worker_no++) {
        std::string identity = std::to_string(worker_no);
        wctxs[identity] =
            std::make_unique<WorkerContext>(identity, db.snapshot(), nullptr);
        std::thread thread(std::ref(*wctxs[identity].get()));
        thread.detach();
    }

    //  Logic of LRU loop
    //  - Poll backend always, frontend only if 1+ worker ready
    //  - If worker replies, queue worker as ready and forward reply
    //    to client if necessary
    //  - If client requests, pop next worker and send request to it
    //
    //  A very simple queue structure with known max size

    for (;;) {
        //  Initialize poll set
        //  Always poll for worker activity on backend
        //  Poll front-end only if we have available workers
        zmq::pollitem_t items[] = {
            {static_cast<void *>(backend), 0, ZMQ_POLLIN, 0},
            {static_cast<void *>(frontend), 0, ZMQ_POLLIN, 0}};
        if (!worker_queue.empty()) {
            zmq::poll(&items[0], 2, -1);
        } else {
            zmq::poll(&items[0], 1, -1);
        }

        //  Handle worker activity on backend
        if ((items[0].revents & ZMQ_POLLIN) != 0) {
            poll_backend();
        }
        if ((items[1].revents & ZMQ_POLLIN) != 0) {
            poll_frontend();
        }
    }
}

void NetworkService::commit_task(WorkerContext *wctx) {
    uint64_t task_ms = get_milli_timestamp() - wctx->task->spec().epoch_ms();
    spdlog::info("TASK: done [{}] (in {}ms)", wctx->task->spec().id(), task_ms);

    db.commit_task(wctx->task->spec(), wctx->task->changes());
    wctx->task = std::nullopt;
}

void NetworkService::handle_response(WorkerContext *wctx) {
    s_recv_padding(&backend, ZMQTRACE);

    auto client_addr{s_recv<std::string>(&backend, ZMQTRACE)};

    s_recv_padding(&backend, ZMQTRACE);

    auto reply{s_recv<std::string>(&backend, ZMQTRACE)};
    s_send(&frontend, client_addr, ZMQTRACE, ZMQ_SNDMORE);
    s_send_padding(&frontend, ZMQTRACE, ZMQ_SNDMORE);
    s_send(&frontend, reply, ZMQTRACE);

    commit_task(wctx);

    std::set<DatabaseSnapshot *> working_snapshots;

    for (const auto &p : wctxs) {
        if (p.second->task != std::nullopt) {
            working_snapshots.insert(&p.second->snap);
        }
    }

    db.collect_garbage(working_snapshots);
}

void NetworkService::poll_backend() {
    //  Queue worker address for LRU routing
    auto worker_addr{s_recv<std::string>(&backend, ZMQTRACE)};
    WorkerContext *wctx = wctxs.at(worker_addr).get();

    s_recv_padding(&backend, ZMQTRACE);

    auto resp_type{s_recv<NetAction>(&backend, ZMQTRACE)};

    switch (resp_type) {
        case NetAction::Response:
            worker_queue.push(worker_addr);
            handle_response(wctx);
            break;
        case NetAction::Ready:
            worker_queue.push(worker_addr);
            break;
    }
}

void NetworkService::poll_frontend() {
    //  Now get next client request, route to LRU worker
    //  Client request is [address][empty][request]
    auto client_addr{s_recv<std::string>(&frontend, ZMQTRACE)};

    s_recv_padding(&frontend, ZMQTRACE);

    auto request{s_recv<std::string>(&frontend, ZMQTRACE)};

    std::vector<DatabaseLock> locks;
    DatabaseSnapshot snapshot = db.snapshot();

    try {
        Command cmd{parse_command(request)};
        locks = std::move(dispatch_locks(cmd, &snapshot));
    } catch (const std::runtime_error &err) {
        s_send(&frontend, client_addr, ZMQTRACE, ZMQ_SNDMORE);
        s_send_padding(&frontend, ZMQTRACE, ZMQ_SNDMORE);
        s_send(&frontend, Response::error("Parse error").to_string(), ZMQTRACE);
        return;
    }

    TaskSpec *spec = nullptr;
    try {
        spec = db.allocate_task(request, client_addr, locks);
    } catch (const std::runtime_error &err) {
        s_send(&frontend, client_addr, ZMQTRACE, ZMQ_SNDMORE);
        s_send_padding(&frontend, ZMQTRACE, ZMQ_SNDMORE);
        s_send(&frontend,
               Response::error("Can't acquire lock, try again later", true)
                   .to_string(),
               ZMQTRACE);
        return;
    }

    Task task{Task(spec)};

    std::string worker_addr = worker_queue.front();
    worker_queue.pop();
    WorkerContext *wctx = wctxs.at(worker_addr).get();
    wctx->task = std::move(task);
    wctx->snap = std::move(snapshot);

    s_send(&backend, worker_addr, ZMQTRACE, ZMQ_SNDMORE);
    s_send_padding(&backend, ZMQTRACE, ZMQ_SNDMORE);
    s_send(&backend, client_addr, ZMQTRACE, ZMQ_SNDMORE);
    s_send_padding(&backend, ZMQTRACE, ZMQ_SNDMORE);
    s_send(&backend, request, ZMQTRACE);
}

void print_usage_and_exit() {
    puts("Usage:");
    puts("    ursadb database-file [bind-address]");
    puts("       -v     Enable verbose output (debug messages)");
    exit(1);
}

// Main entry point of the system!
int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage_and_exit();
    }

    spdlog::info("UrsaDB v{}", get_version_string());

    fix_rlimit();
    migrate_version(argv[1]);

    try {
        bool enable_debug = false;
        const char *db_file = NULL;
        const char *bind_address = NULL;
        for (int i = 1; i < argc; i++) {
            if (std::string("-v") == argv[i]) {
                enable_debug = true;
            } else if (db_file == NULL) {
                db_file = argv[i];  // first positional arg
            } else if (bind_address == NULL) {
                bind_address = argv[i];
            } else {
                spdlog::error("Too many positional arguments.");
            }
        }

        if (db_file == NULL) {
            print_usage_and_exit();
        } else if (bind_address == NULL) {
            bind_address = "tcp://127.0.0.1:9281";
        }

        if (enable_debug) {
            spdlog::set_level(spdlog::level::debug);
            spdlog::debug("Debug logging enabled");
        }

        Database db(db_file);
        spdlog::info("BIND: {}", bind_address);
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
