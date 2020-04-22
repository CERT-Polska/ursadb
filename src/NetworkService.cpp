#include "NetworkService.h"

#include <thread>

#include "Daemon.h"
#include "libursa/DatabaseHandle.h"
#include "libursa/Responses.h"
#include "libursa/ZHelpers.h"
#include "spdlog/spdlog.h"

[[noreturn]] void WorkerContext::operator()() {
    zmq::context_t context(1);
    zmq::socket_t worker(context, ZMQ_REQ);

    worker.setsockopt(ZMQ_IDENTITY, identity.c_str(), identity.length());
    worker.connect(std::string(BACKEND_SOCKET));

    //  Tell backend we're ready for work
    s_send<NetAction>(&worker, NetAction::Ready);

    for (;;) {
        //  Read and save all frames until we get an empty frame
        //  In this example there is only 1 but it could be more
        auto [address, request] =
            s_recv_message<std::string, std::string>(&worker);
        spdlog::info("Task {} - {}", task->id, request);

        snap.set_db_handle(DatabaseHandle(&worker));

        Response response = dispatch_command_safe(request, task, &snap);
        // Note: optionally add funny metadata to response here (like request
        // time, assigned worker, etc)
        s_send_message(&worker, std::make_tuple(NetAction::Response, address,
                                                response.to_string()));
    }
}

void NetworkService::run() {
    constexpr std::string backend_socket{"ipc://backend.ipc"};
    
    for (int worker_no = 0; worker_no < NUM_WORKERS; worker_no++) {
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
        if (worker_queue.size()) {
            zmq::poll(&items[0], 2, -1);
        } else {
            zmq::poll(&items[0], 1, -1);
        }

        //  Handle worker activity on backend
        if (items[0].revents & ZMQ_POLLIN) {
            poll_backend();
        }
        if (items[1].revents & ZMQ_POLLIN) {
            poll_frontend();
        }
    }
}

void NetworkService::commit_task(WorkerContext *wctx) {
    auto did_task = wctx->task->id;
    uint64_t task_ms = get_milli_timestamp() - wctx->task->epoch_ms;
    spdlog::info("Task {} finished by {} in {}", did_task, wctx->identity,
                 task_ms);

    db.commit_task(did_task);
    wctx->task = nullptr;
}

void NetworkService::handle_dataset_lock_req(WorkerContext *wctx,
                                             const std::string &worker_addr) {
    std::vector<std::string> ds_names;
    std::string recv_ds_name;

    do {
        s_recv_padding(&backend);

        recv_ds_name = s_recv<std::string>(&backend);
        ds_names.push_back(recv_ds_name);
    } while (!recv_ds_name.empty());

    s_send(&backend, worker_addr, ZMQ_SNDMORE);
    s_send_padding(&backend, ZMQ_SNDMORE);

    bool already_locked = false;
    for (const std::string &ds_name : ds_names) {
        for (const auto &p : wctxs) {
            if (p.second->task != nullptr &&
                p.second->snap.is_dataset_locked(ds_name)) {
                already_locked = true;
                break;
            }
        }
    }

    if (!already_locked) {
        for (const std::string &ds_name : ds_names) {
            wctx->snap.lock_dataset(ds_name);
            spdlog::info("Coordinator: dataset {} locked", ds_name);
        }

        s_send<NetLockResp>(&backend, NetLockResp::LockOk);
    } else {
        spdlog::warn("Coordinator: dataset lock denied");
        s_send<NetLockResp>(&backend, NetLockResp::LockDenied);
    }
}

void NetworkService::handle_iterator_lock_req(WorkerContext *wctx,
                                              const std::string &worker_addr) {
    s_recv_padding(&backend);

    auto iterator_name{s_recv<std::string>(&backend)};

    s_recv_padding(&backend);

    s_send(&backend, worker_addr, ZMQ_SNDMORE);
    s_send_padding(&backend, ZMQ_SNDMORE);

    bool already_locked = false;

    for (const auto &p : wctxs) {
        if (p.second->task != nullptr &&
            p.second->snap.is_iterator_locked(iterator_name)) {
            already_locked = true;
            break;
        }
    }

    if (!already_locked) {
        wctx->snap.lock_iterator(iterator_name);

        spdlog::info("Coordinator: iterator {} locked", iterator_name);
        s_send<NetLockResp>(&backend, NetLockResp::LockOk);
    } else {
        spdlog::info("Coordinator: lock denied for iterator {}", iterator_name);
        s_send<NetLockResp>(&backend, NetLockResp::LockDenied);
    }
}

void NetworkService::handle_response(WorkerContext *wctx) {
    s_recv_padding(&backend);

    auto [client_addr, reply] =
        s_recv_message<std::string, std::string>(&backend);
    s_send_message(&frontend, std::make_tuple(client_addr, reply));

    commit_task(wctx);

    std::set<DatabaseSnapshot *> working_snapshots;

    for (const auto &p : wctxs) {
        if (p.second->task != nullptr) {
            working_snapshots.insert(&p.second->snap);
        }
    }

    db.collect_garbage(working_snapshots);
}

void NetworkService::poll_backend() {
    //  Queue worker address for LRU routing
    auto [worker_addr, resp_type] =
        s_recv_message<std::string, NetAction>(&backend);
    WorkerContext *wctx = wctxs.at(worker_addr).get();

    switch (resp_type) {
        case NetAction::DatasetLockReq:
            handle_dataset_lock_req(wctx, worker_addr);
            break;
        case NetAction::IteratorLockReq:
            handle_iterator_lock_req(wctx, worker_addr);
            break;
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
    auto [client_addr, request] =
        s_recv_message<std::string, std::string>(&frontend);

    std::string worker_addr = worker_queue.front();  // worker_queue [0];
    worker_queue.pop();

    WorkerContext *wctx = wctxs.at(worker_addr).get();

    wctx->task = db.allocate_task(request, client_addr);
    wctx->snap = db.snapshot();

    s_send_message(&backend,
                   std::make_tuple(worker_addr, client_addr, request));
}
