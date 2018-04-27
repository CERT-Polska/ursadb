#pragma once

#include <zmq.hpp>

#include <assert.h>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

//  Provide random number from 0..(num-1)
#define within(num) (int)((float)(num)*random() / (RAND_MAX + 1.0))

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

    bool rc = socket.send(message);
    return (rc);
}

//  Sends string as 0MQ string, as multipart non-terminal
bool s_sendmore(zmq::socket_t &socket, const std::string &string) {

    zmq::message_t message(string.size());
    memcpy(message.data(), string.data(), string.size());

    bool rc = socket.send(message, ZMQ_SNDMORE);
    return (rc);
}

//  Receives all message parts from socket, prints neatly
void s_dump(zmq::socket_t &socket) {
    std::cout << "----------------------------------------" << std::endl;

    while (1) {
        //  Process all parts of the message
        zmq::message_t message;
        socket.recv(&message);

        //  Dump the message as text or binary
        int size = message.size();
        std::string data(static_cast<char *>(message.data()), size);

        bool is_text = true;

        int char_nbr;
        unsigned char byte;
        for (char_nbr = 0; char_nbr < size; char_nbr++) {
            byte = data[char_nbr];
            if (byte < 32 || byte > 127)
                is_text = false;
        }
        std::cout << "[" << std::setfill('0') << std::setw(3) << size << "]";
        for (char_nbr = 0; char_nbr < size; char_nbr++) {
            if (is_text)
                std::cout << (char)data[char_nbr];
            else
                std::cout << std::setfill('0') << std::setw(2) << std::hex
                          << (unsigned int)data[char_nbr];
        }
        std::cout << std::endl;

        int more = 0; //  Multipart detection
        size_t more_size = sizeof(more);
        socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
        if (!more)
            break; //  Last message part
    }
}

//  Set simple random printable identity on socket
//  Caution:
//    DO NOT call this version of s_set_id from multiple threads on MS Windows
//    since s_set_id will call rand() on MS Windows. rand(), however, is not
//    reentrant or thread-safe. See issue #521.
inline std::string s_set_id(zmq::socket_t &socket) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << within(0x10000) << "-"
       << std::setw(4) << std::setfill('0') << within(0x10000);
    socket.setsockopt(ZMQ_IDENTITY, ss.str().c_str(), ss.str().length());
    return ss.str();
}

//  Report 0MQ version number
void s_version(void) {
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    std::cout << "Current 0MQ version is " << major << "." << minor << "." << patch << std::endl;
}

void s_version_assert(int want_major, int want_minor) {
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    if (major < want_major || (major == want_major && minor < want_minor)) {
        std::cout << "Current 0MQ version is " << major << "." << minor << std::endl;
        std::cout << "Application needs at least " << want_major << "." << want_minor
                  << " - cannot continue" << std::endl;
        exit(EXIT_FAILURE);
    }
}

//  Return current system clock as milliseconds
int64_t s_clock(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

//  Sleep for a number of milliseconds
void s_sleep(int msecs) {
    struct timespec t;
    t.tv_sec = msecs / 1000;
    t.tv_nsec = (msecs % 1000) * 1000000;
    nanosleep(&t, NULL);
}

void s_console(const char *format, ...) {
    time_t curtime = time(NULL);
    struct tm *loctime = localtime(&curtime);
    char *formatted = new char[20];
    strftime(formatted, 20, "%y-%m-%d %H:%M:%S ", loctime);
    printf("%s", formatted);
    delete[] formatted;

    va_list argptr;
    va_start(argptr, format);
    vprintf(format, argptr);
    va_end(argptr);
    printf("\n");
}

//  ---------------------------------------------------------------------
//  Signal handling
//
//  Call s_catch_signals() in your application at startup, and then exit
//  your main loop if s_interrupted is ever 1. Works especially well with
//  zmq_poll.

int s_interrupted = 0;
void s_signal_handler(int signal_value) { s_interrupted = 1; }

void s_catch_signals() {
    struct sigaction action;
    action.sa_handler = s_signal_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}