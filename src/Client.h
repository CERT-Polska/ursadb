#pragma once

#include <zmq.hpp>

class UrsaClient {
private:
    bool command_active = false;
    bool terminated = false;

    std::string server_addr;
    std::string db_command;

    std::string server_version;
    std::string connection_id;

    void status_worker();
    void init_conn(zmq::socket_t& socket);
    void recv_res(zmq::socket_t& socket);

public:
    UrsaClient(std::string server_addr, std::string db_command);
    int start();
};

