#pragma once

#include <sys/types.h>

#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <queue>
#include <sstream>
#include <stack>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "libursa/Database.h"

constexpr int NUM_WORKERS = 4;

class WorkerContext {
   public:
    std::string identity;
    DatabaseSnapshot snap;
    Task *task;

    WorkerContext(const WorkerContext &) = delete;
    WorkerContext(std::string identity, DatabaseSnapshot snap, Task *task)
        : identity(identity), snap(snap), task(task) {}
    [[noreturn]] void operator()();
};

class NetworkService {
    Database &db;
    zmq::context_t context;
    zmq::socket_t frontend;
    zmq::socket_t backend;

    std::map<std::string, std::unique_ptr<WorkerContext>> wctxs;
    std::queue<std::string> worker_queue;
    std::map<std::string, uint64_t> worker_task_ids;

    void poll_frontend();
    void poll_backend();
    void commit_task(WorkerContext *wctx);
    void handle_dataset_lock_req(WorkerContext *wctx,
                                 const std::string &worker_addr);
    void handle_iterator_lock_req(WorkerContext *wctx,
                                  const std::string &worker_addr);
    void handle_response(WorkerContext *wctx);

   public:
    NetworkService(Database &db, const std::string &bind_address)
        : db(db),
          context(1),
          frontend(context, ZMQ_ROUTER),
          backend(context, ZMQ_ROUTER) {
        frontend.bind(bind_address);
        backend.bind("ipc://backend.ipc");
    }
    [[noreturn]] void run();
};
