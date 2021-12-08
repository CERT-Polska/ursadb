#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <type_traits>
#include <zmq.hpp>

// ZMQTRACE is a "magic" parameter that should be passed as std::string_view
// parameter to functions in this module. It expands to the current line
// and filename, which will help in debugging tough zmq errors.
#define STR1(x) #x
#define STR(x) STR1(x)
#define ZMQTRACE "trace(" __FILE__ "," STR(__LINE__) ")"

// Sends one message to socket or throws std::runtime_error if it fails
bool s_send_raw(zmq::socket_t *socket, std::string_view payload, int flags);

// Receives one message from socket or throws std::runtime_error if it fails
std::optional<std::string> s_recv_raw(zmq::socket_t *socket);

template <typename T>
struct is_transferable
    : std::integral_constant<bool, std::is_trivial<T>::value &&
                                       !std::is_array<T>::value> {};

// Receives some POD T from socket
template <typename T>
std::optional<T> s_try_recv(zmq::socket_t *socket) {
    static_assert(is_transferable<T>::value, "Cannot transfer this type");
    auto response{s_recv_raw(socket)};
    if (!response.has_value()) {
        return std::nullopt;
    }

    T val{};
    assert(sizeof(T) == response->size());
    ::memcpy(&val, response->data(), response->size());
    return val;
}

template <>
std::optional<std::string> s_try_recv(zmq::socket_t *socket);

// Sends some POD T over socket
template <typename T>
bool s_try_send(zmq::socket_t *socket, const T &value, int flags = 0) {
    static_assert(is_transferable<T>::value, "Cannot transfer this type");
    std::string msg;
    msg.resize(sizeof(T));
    ::memcpy(msg.data(), &value, msg.size());

    return s_send_raw(socket, std::string_view(msg), flags);
}

template <>
bool s_try_send(zmq::socket_t *socket, const std::string &val, int flags);

template <>
bool s_try_send(zmq::socket_t *socket, const std::string_view &val, int flags);

// Sends a zero-sized frame.
void s_send_padding(zmq::socket_t *socket, std::string_view trace,
                    int flags = 0);
// Receives a zero-sized frame or throws std::runtime_error.
void s_recv_padding(zmq::socket_t *socket, std::string_view trace);

template <typename T>
void s_send(zmq::socket_t *socket, const T &value, std::string_view trace,
            int flags = 0) {
    if (!s_try_send<T>(socket, value, flags)) {
        throw std::runtime_error("s_send failed " + std::string(trace));
    }
}

template <typename T>
T s_recv(zmq::socket_t *socket, std::string_view trace) {
    auto response{s_try_recv<T>(socket)};
    if (!response.has_value()) {
        throw std::runtime_error("s_recv failed" + std::string(trace));
    }
    return *response;
}
