#include "NetworkService.h"

#include "Daemon.h"
#include "zhelpers.h"


static void *worker_thread(void *arg) {
    auto *wctx = static_cast<WorkerContext *>(arg);

    zmq::context_t context(1);
    zmq::socket_t worker(context, ZMQ_REQ);

    worker.setsockopt(ZMQ_IDENTITY, wctx->identity.c_str(), wctx->identity.length());
    worker.connect("ipc://backend.ipc");

    //  Tell backend we're ready for work
    s_send(worker, "READY");

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

        std::string s = dispatch_command_safe(request, wctx->task, &wctx->snap);

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

    const auto &changes = db.current_tasks().at(did_task).get()->changes;
    for (const auto &change : changes) {
        if (change.type == DbChangeType::Insert) {
            db.load_dataset(change.obj_name);
        } else if (change.type == DbChangeType::Drop
                ) {
            db.drop_dataset(change.obj_name);
        } else {
            throw std::runtime_error("unknown change type requested");
        }

        std::cout << "change: " << db_change_to_string(change.type) << " " << change.obj_name << std::endl;
    }

    if (!changes.empty()) {
        db.save();
    }

    db.erase_task(did_task);
    wctx->task = nullptr;
}

void NetworkService::collect_garbage() {
    std::set<const OnDiskDataset *> required_datasets;
    for (const auto *ds : db.working_sets()) {
        required_datasets.insert(ds);
    }

    for (const auto &p : wctxs) {
        if (p.second->task == nullptr) {
            // this worker is not doing anything at the moment
            continue;
        }

        for (const auto *ds : p.second->snap.get_datasets()) {
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

void NetworkService::poll_backend() {
    //  Queue worker address for LRU routing
    std::string worker_addr = s_recv(backend);
    WorkerContext *wctx = wctxs.at(worker_addr).get();

    if (wctx->task != nullptr) {
        commit_task(wctx);
        collect_garbage();
    }

    worker_queue.push(worker_addr);

    //  Second frame is empty
    if (s_recv(backend).size() != 0) {
        throw std::runtime_error("Expected zero-size frame");
    }

    //  Third frame is READY or else a client reply address
    std::string client_addr = s_recv(backend);

    //  If client reply, send rest back to frontend
    if (client_addr.compare("READY") != 0) {
        if (s_recv(backend).size() != 0) {
            throw std::runtime_error("Expected zero-size frame");
        }

        std::string reply = s_recv(backend);
        s_send(frontend, client_addr, ZMQ_SNDMORE);
        s_send(frontend, "", ZMQ_SNDMORE);
        s_send(frontend, reply);
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

    wctx->task = db.allocate_task();
    wctx->snap = db.snapshot();

    s_send(backend, worker_addr, ZMQ_SNDMORE);
    s_send(backend, "", ZMQ_SNDMORE);
    s_send(backend, client_addr, ZMQ_SNDMORE);
    s_send(backend, "", ZMQ_SNDMORE);
    s_send(backend, request);
}