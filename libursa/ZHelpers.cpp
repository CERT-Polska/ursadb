#include "ZHelpers.h"

std::string s_recv(zmq::socket_t &socket) {
    zmq::message_t message;
    socket.recv(&message);
    return std::string(static_cast<char *>(message.data()), message.size());
}

bool s_send(zmq::socket_t &socket, const std::string &string, int flags) {
    zmq::message_t message(string.size());
    memcpy(message.data(), string.data(), string.size());
    return socket.send(message, flags);
}
