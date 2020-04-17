#pragma once

#include <cassert>
#include <string>
#include <type_traits>
#include <zmq.hpp>

// Sends one message to socket or throws std::runtime_error if it fails
void s_send_raw(zmq::socket_t *socket, std::string_view data, int flags);

// Receives one message from socket or throws std::runtime_error if it fails
std::string s_recv_raw(zmq::socket_t *socket);

template <typename T>
struct is_transferable
    : std::integral_constant<bool, std::is_trivial<T>::value &&
                                       !std::is_array<T>::value> {};

// Receives some POD T from socket
template <typename T>
T s_recv(zmq::socket_t *socket) {
    static_assert(is_transferable<T>::value, "Cannot transfer this type");
    std::string response{s_recv_raw(socket)};

    T val{};
    assert(sizeof(T) == response.size());
    ::memcpy(&val, response.data(), response.size());
    return val;
}

// Sends some POD T over socket
template <typename T>
void s_send(zmq::socket_t *socket, const T &value, int flags = 0) {
    static_assert(is_transferable<T>::value, "Cannot transfer this type");
    std::string msg;
    msg.resize(sizeof(T));
    ::memcpy(msg.data(), &value, msg.size());

    s_send_raw(socket, std::string_view(msg), flags);
}

// Sends a zero-sized frame.
void s_send_padding(zmq::socket_t *socket, int flags = 0);
// Receives a zero-sized frame or throws std::runtime_error.
void s_recv_padding(zmq::socket_t *socket);

// Sends std::string over socket
template <>
void s_send(zmq::socket_t *socket, const std::string &value, int flags);
// Receives std::string from socket
template <>
std::string s_recv(zmq::socket_t *socket);

// Sends std::string_view over socket
template <>
void s_send(zmq::socket_t *socket, const std::string_view &value, int flags);
