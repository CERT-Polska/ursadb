#include "NetworkService.h"

#include "Daemon.h"
#include "ZHelpers.h"
#include "DatabaseHandle.h"


static void *worker_thread(void *arg) {
    auto *wctx = static_cast<WorkerContext *>(arg);

    zmq::context_t context(1);
    zmq::socket_t worker(context, ZMQ_REQ);

    worker.setsockopt(ZMQ_IDENTITY, wctx->identity.c_str(), wctx->identity.length());
    worker.connect("ipc://backend.ipc");

    //  Tell backend we're ready for work
    s_send_val<NetAction>(worker, NetAction::Ready);

    for (;;) {
        //  Read and save all frames until we get an empty frame
        //  In this example there is only 1 but it could be more
        std::string address = s_recv(worker);
        if (s_recv(worker).size() != 0) {
            throw std::runtime_error("Expected zero-size frame after address");
        }

        //  Get request, send reply
        std::string request = s_recv(worker);
        std::cout << "Worker: " << request << std::endl;

        wctx->snap.set_db_handle(DatabaseHandle(&worker));

        std::string s = dispatch_command_safe(request, wctx->task, &wctx->snap);

        s_send_val<NetAction>(worker, NetAction::Response, ZMQ_SNDMORE);
        s_send(worker, "", ZMQ_SNDMORE);
        s_send(worker, address, ZMQ_SNDMORE);
        s_send(worker, "", ZMQ_SNDMORE);
        s_send(worker, s);
    }
    return (NULL);
}

void NetworkService::run() {
    for (int worker_no = 0; worker_no < NUM_WORKERS; worker_no++) {
        std::string identity = std::to_string(worker_no);
        wctxs[identity] = std::make_unique<WorkerContext>(identity, db.snapshot(), nullptr);
        pthread_t worker;
        pthread_create(&worker, nullptr, worker_thread, (void *)wctxs[identity].get());
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
        zmq::pollitem_t items[] = {{static_cast<void *>(backend), 0, ZMQ_POLLIN, 0},
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
    uint64_t did_task = wctx->task->id;
    std::cout << "worker finished: " << wctx->identity << ", he was doing task " << did_task
              << std::endl;

    db.commit_task(wctx->task->id);
    wctx->task = nullptr;
}

void NetworkService::handle_lock_req(WorkerContext *wctx, const std::string &worker_addr) {
    std::vector<std::string> ds_names;
    std::string recv_ds_name;

    do {
        if (s_recv(backend).size() != 0) {
            throw std::runtime_error("Expected zero-size frame");
        }

        recv_ds_name = s_recv(backend);
        ds_names.push_back(recv_ds_name);
    } while (!recv_ds_name.empty());

    s_send(backend, worker_addr, ZMQ_SNDMORE);
    s_send(backend, "", ZMQ_SNDMORE);

    bool already_locked = false;

    for (const std::string &ds_name : ds_names) {
        for (const auto &p : wctxs) {
            if (p.second->task != nullptr && p.second->snap.is_locked(ds_name)) {
                already_locked = true;
                break;
            }
        }
    }

    if (!already_locked) {
        for (const std::string &ds_name : ds_names) {
            wctx->snap.lock_dataset(ds_name);
        }

        std::cout << "coordinator: locked ok" << std::endl;
        s_send_val<NetLockResp>(backend, NetLockResp::LockOk);
    } else {
        std::cout << "coordinator: lock denied" << std::endl;
        s_send_val<NetLockResp>(backend, NetLockResp::LockDenied);
    }
}

void NetworkService::handle_response(WorkerContext *wctx) {
    if (s_recv(backend).size() != 0) {
        throw std::runtime_error("Expected zero-size frame");
    }

    std::string client_addr = s_recv(backend);

    if (s_recv(backend).size() != 0) {
        throw std::runtime_error("Expected zero-size frame");
    }

    std::string reply = s_recv(backend);
    s_send(frontend, client_addr, ZMQ_SNDMORE);
    s_send(frontend, "", ZMQ_SNDMORE);
    s_send(frontend, reply);

    commit_task(wctx);

    std::set<DatabaseSnapshot*> working_snapshots;

    for (const auto &p : wctxs) {
        if (p.second->task != nullptr) {
            working_snapshots.insert(&p.second->snap);
        }
    }

    db.collect_garbage(working_snapshots);
}

void NetworkService::poll_backend() {
    //  Queue worker address for LRU routing
    std::string worker_addr = s_recv(backend);
    WorkerContext *wctx = wctxs.at(worker_addr).get();

    if (s_recv(backend).size() != 0) {
        throw std::runtime_error("Expected zero-size frame");
    }

    auto resp_type = s_recv_val<NetAction>(backend);

    switch (resp_type) {
        case NetAction::LockReq:
            handle_lock_req(wctx, worker_addr);
            break;
        case NetAction::Response:
            handle_response(wctx);
        /* fall through */
        case NetAction::Ready:
            worker_queue.push(worker_addr);
    }
}

void NetworkService::poll_frontend() {
    //  Now get next client request, route to LRU worker
    //  Client request is [address][empty][request]
    std::string client_addr = s_recv(frontend);

    if (s_recv(frontend).size() != 0) {
        throw std::runtime_error("Expected zero-size frame");
    }

    std::string request = s_recv(frontend);

    std::string worker_addr = worker_queue.front(); // worker_queue [0];
    worker_queue.pop();

    WorkerContext *wctx = wctxs.at(worker_addr).get();

    wctx->task = db.allocate_task(request, client_addr);
    wctx->snap = db.snapshot();

    s_send(backend, worker_addr, ZMQ_SNDMORE);
    s_send(backend, "", ZMQ_SNDMORE);
    s_send(backend, client_addr, ZMQ_SNDMORE);
    s_send(backend, "", ZMQ_SNDMORE);
    s_send(backend, request);
}