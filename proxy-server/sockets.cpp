//
//  sockets.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 26.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <sys/socket.h> /* Includes for all the network headers */
#include <arpa/inet.h>

#include <exception>
#include <vector>
#include <cassert>
#include "sockets.hpp"

/******** SOCKET ********/

socket_wrap::socket_wrap(socket_wrap const& other) {
    fd = other.fd;
}

socket_wrap::socket_wrap() {}

socket_wrap::socket_wrap(int fd) :
    fd(fd)
{}

socket_wrap socket_wrap::connect(int accept_fd) {
    socket_wrap temp;
    
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    
    temp.fd = accept(accept_fd, (sockaddr*) &addr, &len);
    if (temp.fd == -1) {
        throw socket_exception("Accepting connection error occurred!");
    }
    
    return temp;
}

void socket_wrap::accept_socket(int accept_fd) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    
    fd = accept(accept_fd, (sockaddr*) &addr, &len);
    if (fd == -1) {
        throw socket_exception("Accepting connection error occurred!");
    }
}

ptrdiff_t socket_wrap::write(std::string const& msg) {
    ptrdiff_t length = send(fd, msg.data(), msg.size(), 0);
    
    if (length == -1) {
        throw socket_exception("Writing message error occurred!");
    }
    
    return length;
};

std::string socket_wrap::read(size_t buffer_size) {
    std::vector<char> buffer(buffer_size);
    ptrdiff_t length = recv(fd, buffer.data(), buffer_size, 0);
    if (length == -1) {
        throw socket_exception("Reading message error occurred!");
    }
    
    return std::string(buffer.cbegin(), buffer.cbegin() + length);
}

int socket_wrap::get_fd() const noexcept {
    return fd;
}

socket_wrap::~socket_wrap() {
    close(fd);
}

/******** CLIENT ********/

client::client(int accept_fd) :
    socket(socket_wrap::connect(accept_fd)),
    server(nullptr)
{}

int client::get_fd() const noexcept {
    return socket.get_fd();
}

int client::get_server_fd() const noexcept {
    assert(server);
    return server->get_fd();
}

bool client::has_server() const noexcept {
    return server;
}

void client::bind(struct server* new_server) {
    delete server;
    server = new_server;
    server->bind(this);
}

void client::unbind() {
    server = nullptr;
}

std::string& client::get_buffer() {
    return buffer;
}

size_t client::get_buffer_size() const noexcept {
    return buffer.length();
}

void client::append(std::string& str) {
    buffer.append(str);
}

bool client::has_capcacity() const noexcept {
    return buffer.size() < BUFFER_SIZE;
}

size_t client::read(size_t size) {
    try {
        std::string s{socket.read(size)};
        buffer.append(s);
        return s.size();
    } catch (...) {
        return 0;
    }
}

size_t client::write() {
    try {
        size_t length = socket.write(buffer);
        buffer = buffer.substr(length);
        if (server)
            get_msg();
        return length;
    } catch (...) {
        return 0;
    }
}

void client::get_msg() {
    assert(server);
    if (buffer.size() < BUFFER_SIZE && server) {
        server->send_msg();
    }
}

void client::send_msg() {
    assert(server);
    server->append(buffer);
    buffer.clear();
}

bool client::check_request_end() const noexcept {
    size_t i = buffer.find("\r\n\r\n");
    
    if (i == std::string::npos)
        return false;
    if (buffer.find("POST") == std::string::npos)
        return true;
    
    size_t j = buffer.find("Content-Length: ");
    j += 16;
    size_t content_length = 0;
    while (buffer[j] != '\r') {
        content_length *= 10;
        content_length += (buffer[j++] - '0');
    }
    
    i += 4;
    return buffer.substr(i).length() == content_length;
}

client::~client() {
    delete server;
}

/******** SERVER ********/

server::server(int fd) :
    socket(fd),
    client(nullptr)
{}

int server::get_fd() const noexcept {
    return socket.get_fd();
}

int server::get_client_fd() const noexcept {
    return client->get_fd();
}

void server::bind(struct client* new_client) {
    client = new_client;
}

void server::append(std::string& str) {
    buffer.append(str);
}

bool server::buffer_empty() const noexcept {
    return buffer.empty();
}

size_t server::read(size_t size) {
    assert(client);
    if (!client->has_capcacity())
        return 0;
    try {
        std::string s{socket.read(size)};
        buffer.append(s);
        return s.size();
    } catch (...) {
        return 0;
    }
}

size_t server::write() {
    try {
        size_t length = socket.write(buffer);
        buffer = buffer.substr(length);
        if (client)
            get_msg();
        return length;
    } catch (...) {
        return 0;
    }
}

void server::get_msg() {
    assert(client);
    if (buffer.size() < BUFFER_SIZE) {
        client->send_msg();
    }
}

void server::send_msg() {
    assert(client);
    client->append(buffer);
    buffer.clear();
}


server::~server() {
    client->unbind();
}