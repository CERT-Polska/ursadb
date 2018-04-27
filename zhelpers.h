#pragma once

#include <zmq.hpp>

#include <string>

//  Receive 0MQ string from socket and convert into string
std::string s_recv(zmq::socket_t &socket) {
    zmq::message_t message;
    socket.recv(&message);
    return std::string(static_cast<char *>(message.data()), message.size());
}

//  Convert string to 0MQ string and send to socket
bool s_send(zmq::socket_t &socket, const std::string &string) {
    zmq::message_t message(string.size());
    memcpy(message.data(), string.data(), string.size());
    return socket.send(message);
}

//  Sends string as 0MQ string, as multipart non-terminal
bool s_sendmore(zmq::socket_t &socket, const std::string &string) {
    zmq::message_t message(string.size());
    memcpy(message.data(), string.data(), string.size());
    return socket.send(message, ZMQ_SNDMORE);
}
