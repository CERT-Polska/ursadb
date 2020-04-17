#include "DatabaseHandle.h"

#include "ZHelpers.h"

DatabaseHandle::DatabaseHandle() : worker(nullptr) {}

DatabaseHandle::DatabaseHandle(zmq::socket_t *worker) : worker(worker) {}

[[nodiscard]] bool DatabaseHandle::request_dataset_lock(
    const std::vector<std::string> &ds_names) const {
    s_send<NetAction>(worker, NetAction::DatasetLockReq, ZMQ_SNDMORE);

    for (const auto &ds_name : ds_names) {
        s_send_padding(worker, ZMQ_SNDMORE);
        s_send(worker, ds_name, ZMQ_SNDMORE);
    }

    s_send_padding(worker, ZMQ_SNDMORE);
    s_send_padding(worker);

    return s_recv<NetLockResp>(worker) == NetLockResp::LockOk;
}

[[nodiscard]] bool DatabaseHandle::request_iterator_lock(
    const std::string &it_name) const {
    s_send(worker, NetAction::IteratorLockReq, ZMQ_SNDMORE);
    s_send_padding(worker, ZMQ_SNDMORE);
    s_send(worker, it_name, ZMQ_SNDMORE);

    s_send_padding(worker);

    return s_recv<NetLockResp>(worker) == NetLockResp::LockOk;
}
