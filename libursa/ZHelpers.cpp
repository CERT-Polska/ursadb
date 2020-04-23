#include "ZHelpers.h"

void s_send_raw(zmq::socket_t *socket, std::string_view payload,
                int flags = 0) {
    zmq::message_t message(payload.size());
    ::memcpy(message.data(), payload.data(), payload.size());
    bool ok{socket->send(message, flags)};
    if (!ok) {
        throw std::runtime_error("zmq socket: send failed");
    }
}

std::string s_recv_raw(zmq::socket_t *socket) {
    zmq::message_t message;
    bool ok{socket->recv(&message)};
    if (!ok) {
        throw std::runtime_error("zmq socket: recv failed");
    }
    return std::string(static_cast<char *>(message.data()), message.size());
}

void s_send_padding(zmq::socket_t *socket, int flags) {
    s_send_raw(socket, std::string_view{}, flags);
}

void s_recv_padding(zmq::socket_t *socket) {
    auto resp{s_recv<std::string>(socket)};
    if (!resp.empty()) {
        throw std::runtime_error("Expected zero-sized frame");
    }
}

template <>
void s_send(zmq::socket_t *socket, const std::string &value, int flags) {
    s_send_raw(socket, std::string_view(value), flags);
}

template <>
std::string s_recv(zmq::socket_t *socket) {
    return s_recv_raw(socket);
}

template <>
void s_send(zmq::socket_t *socket, const std::string_view &value, int flags) {
    s_send_raw(socket, value, flags);
}
