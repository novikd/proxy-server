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
//#include "handlers.hpp"
#include "event_queue.hpp"
#include "sockets.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <mutex>
#include <fcntl.h>

struct proxy_server {
    
    proxy_server(int);

    void run();
    
    void start();
    void hard_stop();
    void stop();
    
    ~proxy_server();
    
    lru_cache<std::string, addrinfo> hosts;
    events_queue queue;
    std::mutex blocker;
//    std::queue<client_wrap*> clients_for_resolver;
    std::condition_variable cv;
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

    std::map<uintptr_t, client*> clients;
    std::map<uintptr_t, server*> servers;
    bool work;
    int main_socket;
    
};


//TODO: handle all the errors

#warning sigterm should stop main_loop and destructors will do everything

#endif /* networking_hpp */
