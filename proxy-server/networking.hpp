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
#include "handlers.hpp"
#include <fcntl.h>

struct proxy_server {
    
    proxy_server(int sock) :
        queue(),
        work(true),
        main_socket(sock)
    {
        struct connector* tmp = new connector(&queue, main_socket);
        queue.add_event(main_socket, EVFILT_READ, EV_ADD, tmp);
        queue.add_event(SIGINT, EVFILT_SIGNAL, EV_ADD, new signal_handler(&queue, tmp)); // TODO: when signal handlers dies
        queue.add_event(SIGTERM, EVFILT_SIGNAL, EV_ADD, new signal_handler(&queue, tmp));
    }

    void run() {
        while(work) {
            int amount = queue.occured();
            
            if (amount == -1) {
                perror("Getting new events error occured!\n");
            }
            queue.execute();
        }
    }
    
    void start() {
        work = true;
    }

    void hard_stop() {
        work = false;
    }

    void stop() {
#warning FIXME: memory leak here
        queue.add_event(main_socket, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    }
    
    lru_cache<std::string, addrinfo> hosts;
    events_queue queue;
    std::mutex blocker;
    std::queue<client_wrap*> clients_for_resolver;
    std::condition_variable cv;
private:
    bool work;
    int main_socket;
};

void resolve_hosts(proxy_server& proxy) {
    while (true) {
        std::unique_lock<std::mutex> locker(proxy.blocker);
        proxy.cv.wait(locker, [&]() {
            return !proxy.clients_for_resolver.empty();
        });
        
        addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        
        client_wrap* client = proxy.clients_for_resolver.front();
        proxy.clients_for_resolver.pop();
        
        if (proxy.hosts.exists(client->host)) {
            result = new addrinfo(proxy.hosts[client->host]);
        } else {
            size_t i = client->host.find(":");
            std::string name(client->host.substr(0, i)), port;
            
            if (i != std::string::npos)
                port = client->host.substr(i);
            
            int error = getaddrinfo(name.c_str(), port.c_str(), &hints, &result);

            if (error) {
                perror("\nRESOLVING ERROR OCCURRED\n");
            } else {
                proxy.hosts.insert(client->host, *result);
            }
        }
        
        int sock = -1;
        sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (sock == -1) {
            perror("\nGETTING SOCKET ERROR OCCURRED\n");
        }
        
        if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
            perror("\nMAKING SOCKET NON-BLOCKING ERROR OCCURRED\n");
        }
        
        if (connect(sock, result->ai_addr, result->ai_addrlen) == -1) {
            perror("\nCONNECTING ERROR OCCURRED\n");
            close(sock);
            sock = -1;
        }
        
        try {
            server_handler* tmp = new server_handler(&proxy.queue, sock);
            client->bind(tmp->get_server()); // TODO: race condition
            tmp->get_server()->bind(client);
            proxy.queue.add_event(sock, EVFILT_WRITE, EV_ADD, 0, 0, tmp);
        } catch (...) {
            perror("\nCREATING SERVER ERROR OCCURRED\n");
        }
        
        freeaddrinfo(result);
    }
}

#warning handler all the errors

#warning sigterm should stop main_loop and destructors will do everything

#endif /* networking_hpp */
