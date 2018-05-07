#pragma once

#include <zmq.hpp>

class DatabaseHandle {
    zmq::socket_t *worker;

public:
    DatabaseHandle();
    DatabaseHandle(zmq::socket_t *worker);
    void request_dataset_lock(const std::string &ds_name) const;
};
