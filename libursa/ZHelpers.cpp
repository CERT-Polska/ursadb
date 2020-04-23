#include "ZHelpers.h"

bool s_send_raw(zmq::socket_t *socket, std::string_view payload,
                int flags = 0) {
    zmq::message_t message(payload.size());
    ::memcpy(message.data(), payload.data(), payload.size());
    return socket->send(message, flags);
}

std::optional<std::string> s_recv_raw(zmq::socket_t *socket) {
    zmq::message_t message;
    if (!socket->recv(&message)) {
        return std::nullopt;
    }
    return std::string(static_cast<char *>(message.data()), message.size());
}

void s_send_padding(zmq::socket_t *socket, int flags) {
    if (!s_send_raw(socket, std::string_view{}, flags)) {
        throw std::runtime_error("s_send_padding failed");
    }
}

void s_recv_padding(zmq::socket_t *socket) {
    auto resp{s_recv<std::string>(socket)};
    if (!resp.empty()) {
        throw std::runtime_error("Expected zero-sized frame");
    }
}

template <>
std::optional<std::string> s_try_recv(zmq::socket_t *socket) {
    return s_recv_raw(socket);
}

template <>
bool s_try_send(zmq::socket_t *socket, const std::string_view &value,
                int flags) {
    return s_send_raw(socket, value, flags);
}

template <>
bool s_try_send(zmq::socket_t *socket, const std::string &value, int flags) {
    return s_send_raw(socket, std::string_view(value), flags);
}
