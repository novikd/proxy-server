//
//  networking.h
//  proxy-server
//
//  Created by Дмитрий Новик on 27.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef networking_hpp
#define networking_hpp

#include "lru_cache.hpp"
#include "event_queue.hpp"
#include "sockets.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <mutex>
#include <fcntl.h>
#include <queue>
#include <iostream>

struct proxy_server {
    
    proxy_server(int);

    void run();
    
    void start();
    void hard_stop();
    void stop();
    
    void resolve_hosts();
    
    ~proxy_server();
    
private:
    static int init_socket(int);
    
    void disconnect_client(struct kevent&);
    void disconnect_server(struct kevent&);
    
    void read_from_client(struct kevent&);
    void read_from_server(struct kevent&);
    void write_to_client(struct kevent&);
    void write_to_server(struct kevent&);
    
    void timeout_exceeded(struct kevent&);

    void connect_client(struct kevent&);
    void on_host_resolved(struct kevent&);

/*********** FIELDS ***********/
    std::map<uintptr_t, client*> clients;
    std::map<uintptr_t, server*> servers;
    int main_socket;
    events_queue queue;
    lru_cache<std::string, addrinfo> hosts;
    std::mutex blocker;
    std::queue<std::pair<uintptr_t, std::string> > hosts_to_resolve;
    std::condition_variable cv;
    bool work;
};

//TODO: handle all the errors
//TODO: sigterm should stop main_loop and destructors will do everything

#endif /* networking_hpp */
