//
//  sockets.hpp
//  proxy-server
//
//  Created by Дмитрий Новик on 23.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef sockets_hpp
#define sockets_hpp

#include <iostream>
#include "socket_exception.hpp"

const uint32_t BUFFER_SIZE = 20000;

struct socket_wrap {

    socket_wrap& operator=(const socket_wrap&) = delete;

    socket_wrap();
    socket_wrap(const socket_wrap&);
    
    socket_wrap(int fd);

    static socket_wrap connect(int);
    void accept_socket(int accept_fd); //TODO: delete this method. connect should do it
    
    ptrdiff_t write(std::string const& msg);
    std::string read(size_t buffer_size);
    int get_fd() const noexcept;
    
    ~socket_wrap();

private:
    int fd;
};

struct server;

struct client {
    client(const client&) = delete;
    client& operator=(client const&) = delete;

    client(int);

    int get_fd() const noexcept;
    int get_server_fd() const noexcept;
    bool has_server() const noexcept;

    void bind(struct server* new_server);
    void unbind();
    
    std::string& get_buffer();
    size_t get_buffer_size() const noexcept;
    void append(std::string&);
    bool has_capcacity() const noexcept;
    
    size_t read(size_t);
    size_t write();
    
    void get_msg();
    void send_msg();
    
    bool check_request_end() const noexcept;

    ~client();
private:
    std::string buffer;
    socket_wrap socket;
    struct server* server;
};

struct server {
    server(const server&) = delete;
    server& operator=(const server&) = delete;
    
    server(int);
    
    int get_fd() const noexcept;
    int get_client_fd() const noexcept;
    void bind(struct client*);
    
    void append(std::string&);
    bool buffer_empty() const noexcept;
    
    size_t read(size_t);
    size_t write();
    
    void send_msg();
    void get_msg();
    
    ~server();
private:
    std::string buffer;
    socket_wrap socket;
    struct client* client;
};

#endif /* sockets_hpp */