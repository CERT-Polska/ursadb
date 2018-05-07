#include "DatabaseHandle.h"

#include "zhelpers.h"


DatabaseHandle::DatabaseHandle() : worker(nullptr) {}

DatabaseHandle::DatabaseHandle(zmq::socket_t *worker) : worker(worker) {}

void DatabaseHandle::request_dataset_lock(const std::string &ds_name) const {
    s_send(*worker, "lock_req", ZMQ_SNDMORE);
    s_send(*worker, "", ZMQ_SNDMORE);
    s_send(*worker, ds_name);

    if (s_recv(*worker) != "lock_ok") {
        throw std::runtime_error("failed to lock dataset");
    }
}
