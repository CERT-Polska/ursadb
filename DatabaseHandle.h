#pragma once

#include <zmq.hpp>

enum NetAction {
    Ready = 0,
    Response = 1,
    LockReq = 2
};

enum NetLockResp {
    LockOk = 0,
    LockDenied = 1
};

class DatabaseHandle {
    zmq::socket_t *worker;

public:
    DatabaseHandle();
    DatabaseHandle(zmq::socket_t *worker);
    void request_dataset_lock(const std::vector<std::string> &ds_names) const;
};
