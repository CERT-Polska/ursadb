#pragma once

#include <string>
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

#include "Database.h"

struct WorkerArgs {
    int worker_nbr;
    Database *db;
    std::map<std::string, DatabaseSnapshot> *snapshots;
};

class NetworkService {
    Database &db;
    zmq::context_t context;
    zmq::socket_t frontend;
    zmq::socket_t backend;

    std::map<std::string, DatabaseSnapshot> snapshots;
    std::queue<std::string> worker_queue;
    std::map<std::string, uint64_t> worker_task_ids;

    void poll_frontend();
    void poll_backend();

public:
    NetworkService(Database &db, const std::string &bind_address)
            : db(db), context(1), frontend(context, ZMQ_ROUTER), backend(context, ZMQ_ROUTER) {
        frontend.bind(bind_address);
        backend.bind("ipc://backend.ipc");
    }
    void run();
};
