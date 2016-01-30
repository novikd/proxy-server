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
#include <fcntl.h>

#include "exceptions.hpp"
#include "sockets.hpp"


namespace sockets {
    int create_connect_socket(sockaddr addr) {
        socket_wrap server_socket{socket(AF_INET, SOCK_STREAM, 0)};
        
        if (server_socket.get_fd() == -1) {
            throw custom_exception("Creating socket for server error occurred!\n");
        }
        
        const int set = 1;
        if (setsockopt(server_socket.get_fd(), SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) {
            throw custom_exception("Setting server socket error occurred!\n");
        };
        
        if (fcntl(server_socket.get_fd(), F_SETFL, O_NONBLOCK) == -1) {
            throw custom_exception("Making socket non-blocking error occurred!\n");
        }

        std::cout << "Connecting to IP: " << inet_ntoa(((sockaddr_in*) &addr)->sin_addr) << "\nSocket: " << server_socket.get_fd() << "\n";
        if (connect(server_socket.get_fd(), &addr, sizeof(addr)) == -1) {
            if (errno != EINPROGRESS) {
                throw custom_exception("\nConnecting error occurred!");
            }
        }
        
        return server_socket.release();
    }
}

/******** DESCRIPTOR ********/

file_descriptor::file_descriptor() :
    fd(-1)
{}

file_descriptor::file_descriptor(file_descriptor&& rhs) :
    fd(rhs.fd)
{
    rhs.fd = -1;
}

file_descriptor::file_descriptor(int fd) :
    fd(fd)
{}

file_descriptor& file_descriptor::operator=(file_descriptor&& rhs) {
    fd = rhs.fd;
    rhs.fd = -1;

    return *this;
}

void file_descriptor::set_fd(int new_fd) noexcept {
    fd = new_fd;
}

int file_descriptor::get_fd() const noexcept {
    return fd;
}

int file_descriptor::release() noexcept {
    int temp = fd;
    fd = -1;
    return temp;
}

file_descriptor::~file_descriptor() {
    if (fd == -1)
        return;
    
    if (close(fd) == -1) {
        perror("Closing file descriptor error occurred!");
    }
    std::cout << "File descriptor: " << fd << " closed!\n";
}

/******** SOCKET ********/

socket_wrap::socket_wrap(int fd) :
    file_descriptor(fd)
{}

socket_wrap socket_wrap::accept(int accept_fd) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    
    int fd = ::accept(accept_fd, (sockaddr*) &addr, &len);
    if (fd == -1)
        throw custom_exception("Connecting error occurred!");
    
    return socket_wrap(fd);
}

ptrdiff_t socket_wrap::write(std::string const& msg) {
    ptrdiff_t length = send(fd, msg.data(), msg.size(), 0);
    
    if (length == -1) {
        throw custom_exception("Writing message error occurred!");
    }
    
    return length;
};

std::string socket_wrap::read(size_t buffer_size) {
    std::vector<char> buffer(buffer_size);
    ptrdiff_t length = recv(fd, buffer.data(), buffer_size, 0);
    if (length == -1) {
        throw custom_exception("Reading message error occurred!");
    }
    
    return std::string(buffer.cbegin(), buffer.cbegin() + length);
}

socket_wrap::~socket_wrap() {}

/******** CLIENT ********/

client::client(int accept_fd) :
    socket(socket_wrap::accept(accept_fd)),
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
    return server.get() != nullptr;
}

void client::bind(struct server* new_server) {
    server.reset(new_server);
    server->bind(this);
}

void client::unbind() {
    server.reset(nullptr);
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
        buffer.erase(buffer.begin(), buffer.begin() + length);
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

std::string client::get_host() const noexcept {
    assert(server);
    return server->get_host();
}

client::~client() {
    if (server)
        unbind();
}

/******** SERVER ********/

server::server(int fd) :
    socket(fd),
    client(nullptr)
{}

server::server(sockaddr addr) :
    socket(sockets::create_connect_socket(addr)),
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

std::string& server::get_buffer() noexcept {
    return buffer;
}

bool server::buffer_empty() const noexcept {
    return buffer.empty();
}

std::string server::read(size_t size) {
    assert(client);
    if (!client->has_capcacity())
        return "";
    try {
        std::string s{socket.read(size)};
        buffer.append(s);
        return s;
    } catch (...) {
        return "";
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

void server::set_host(std::string const& str) {
    host = str;
}

std::string server::get_host() const noexcept {
    return host;
}

void server::disconnect() {
    assert(client);
    client->unbind();
}

server::~server() {}
