#include "DatabaseHandle.h"

#include "zhelpers.h"


DatabaseHandle::DatabaseHandle() : worker(nullptr) {}

DatabaseHandle::DatabaseHandle(zmq::socket_t *worker) : worker(worker) {}

void DatabaseHandle::request_dataset_lock(const std::vector<std::string> &ds_names) const {
    s_send(*worker, "lock_req", ZMQ_SNDMORE);

    for (const auto &ds_name : ds_names) {
        s_send(*worker, "", ZMQ_SNDMORE);
        s_send(*worker, ds_name, ZMQ_SNDMORE);
    }

    s_send(*worker, "", ZMQ_SNDMORE);
    s_send(*worker, "");

    if (s_recv(*worker) != "lock_ok") {
        throw std::runtime_error("failed to lock dataset");
    }
}
