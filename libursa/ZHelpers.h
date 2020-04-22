#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <zmq.hpp>

// Sends one message to socket or throws std::runtime_error if it fails
bool s_send_raw(zmq::socket_t *socket, std::string_view data, int flags);

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
void s_send_padding(zmq::socket_t *socket, int flags = 0);
// Receives a zero-sized frame or throws std::runtime_error.
void s_recv_padding(zmq::socket_t *socket);

template <typename T>
void s_send(zmq::socket_t *socket, const T &value, int flags = 0) {
    if (!s_try_send<T>(socket, value, flags)) {
        throw std::runtime_error("s_send failed");
    }
}

template <typename T>
T s_recv(zmq::socket_t *socket) {
    auto response{s_try_recv<T>(socket)};
    if (!response.has_value()) {
        throw std::runtime_error("s_recv failed");
    }
    return *response;
}

// Sends std::string_view over socket
template <>
void s_send(zmq::socket_t *socket, const std::string_view &value, int flags);

template <int Index, int Size, typename T>
void s_send_helper(zmq::socket_t *socket, const T &t) {
    constexpr bool is_last = Index + 1 == Size;
    constexpr bool is_first = Index == 0;

    if constexpr (Index < Size) {
        if constexpr (!is_first) {
            s_send_padding(socket, ZMQ_SNDMORE);
        }
        int flags{is_last ? 0 : ZMQ_SNDMORE};
        s_send(socket, std::get<Index>(t), flags);
        s_send_helper<Index + 1, Size, T>(socket, t);
    }
}

template <typename... Ts>
void s_send_message(zmq::socket_t *socket, const std::tuple<Ts...> &args) {
    using Ttype = std::tuple<Ts...>;
    s_send_helper<0, std::tuple_size_v<Ttype>>(socket, args);
}

template <int Index, int Size, typename T, typename... Ts>
std::tuple<T, Ts...> s_recv_helper(zmq::socket_t *socket) {
    if constexpr (Index != 0) {
        s_recv_padding(socket);
    }
    auto val = s_recv<T>(socket);
    if constexpr (Index + 1 == Size) {
        return std::make_tuple(val);
    } else {
        auto rest = s_recv_helper<Index + 1, Size, Ts...>(socket);
        return std::tuple_cat(std::make_tuple(val), rest);
    }
}

template <typename... Ts>
std::tuple<Ts...> s_recv_message(zmq::socket_t *socket) {
    using Ttype = std::tuple<Ts...>;
    return s_recv_helper<0, std::tuple_size_v<Ttype>, Ts...>(socket);
}
