#pragma once

#include <atomic>

#include <zmq.hpp>

class UrsaClient {
private:
    std::atomic<bool> command_active = false;
    std::atomic<bool> terminated = false;
    std::atomic<uint64_t> wait_time = 0;

    std::string server_addr;
    std::string db_command;
    bool quiet_mode;

    std::string server_version;
    std::string connection_id;

    bool wait_sec();
    void status_worker();
    void init_conn(zmq::socket_t& socket);
    void recv_res(zmq::socket_t& socket);

public:
    UrsaClient(std::string server_addr, std::string db_command, bool silent_mode);
    int start();
};

