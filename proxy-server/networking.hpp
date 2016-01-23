//
//  networking.h
//  proxy-server
//
//  Created by Дмитрий Новик on 27.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef networking_hpp
#define networking_hpp

#include "event_queue.hpp"
#include "sockets.hpp"
#include "http.hpp"
#include "host_resolver.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <mutex>
#include <fcntl.h>
#include <queue>
#include <iostream>

struct proxy_server {

    proxy_server(int);
    proxy_server(proxy_server const&) = delete;
    proxy_server& operator=(proxy_server const&) = delete;

    void run();
    
    void start();
    void hard_stop();
    void stop();
    
    // TODO: typo
    void handle_sugnal(int, std::function<void(struct kevent&)>);
    
    ~proxy_server();
private:
    
    void disconnect_client(struct kevent&);
    void disconnect_server(struct kevent&);
    
    void read_from_client(struct kevent&);
    void read_header_from_server(struct kevent&);
    void read_from_server(struct kevent&);
    void write_to_client(struct kevent&);
    void write_to_server(struct kevent&);

    void connect_client(struct kevent&);
    void on_host_resolved(struct kevent&);
    
    void update_timer(int);

/*********** FIELDS ***********/

    std::map<uintptr_t, client*> clients; // TODO: replace with unique_ptr
    std::map<uintptr_t, server*> servers;
    int main_socket, pipe_fd; // TODO: wrap into a RAII class
    
    events_queue queue;
    bool work, stoped; // TODO: typo
    
    host_resolver resolver;
    std::map<int, http_request*> requests;
    std::map<int, http_response> responses;

    lru_cache<std::string, http_response> cache;
};

#endif /* networking_hpp */
