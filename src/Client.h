#pragma once

#include <atomic>
#include <zmq.hpp>

class UrsaClient {
   private:
    std::string server_addr;
    bool is_interactive;
    bool raw_json;

    std::string server_version;
    std::string connection_id;

    zmq::context_t context;
    zmq::socket_t cmd_socket;
    zmq::socket_t status_socket;

    void check_task_status(const std::string &conn_id);
    void check_conn_status(zmq::socket_t *socket);
    void recv_res(zmq::socket_t *socket);
    void setup_connection();

   public:
    UrsaClient(std::string server_addr, bool is_interactive, bool raw_json);
    void start();
    void one_shot_command(const std::string &cmd);
};
