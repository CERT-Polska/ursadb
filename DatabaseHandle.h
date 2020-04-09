#pragma once

#include <zmq.hpp>

enum class NetAction : uint32_t {
    Ready = 0,
    Response = 1,
    DatasetLockReq = 2,
    IteratorLockReq = 3
};

enum class NetLockResp : uint32_t {
    LockOk = 0,
    LockDenied = 1
};

class DatabaseHandle {
    zmq::socket_t *worker;

public:
    DatabaseHandle();
    DatabaseHandle(zmq::socket_t *worker);
    [[nodiscard]] bool request_dataset_lock(const std::vector<std::string> &ds_names) const;
    [[nodiscard]] bool request_iterator_lock(const std::string &it_name) const;
};
