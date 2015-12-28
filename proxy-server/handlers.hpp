//
//  handlers.hpp
//  proxy-server
//
//  Created by Дмитрий Новик on 28.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef handlers_h
#define handlers_h

#include "event_queue.hpp"
#include "sockets.hpp"
#include "http.hpp"

#include <sys/socket.h>
#include <netdb.h>

struct client_handler : handler {
    client_handler(events_queue* new_queue, int fd) :
        queue(new_queue)
    {
        client = new struct client(fd);
    }
    
    client_handler(client_handler const& other) :
        queue(other.queue),
        client(other.client)
    {}
    
    void handle(struct kevent& event) {
        if (event.flags & EV_EOF && event.data == 0) {
            if (client->has_server())
                queue->invalidate_events(client->get_server_fd());
            queue->invalidate_events(client->get_fd());
            delete_client();
            return;
        }
        if (event.filter == EVFILT_READ) {
            client->read(event.data);
            if (client->check_request_end()) {
                std::string s = client->get_buffer();
                request query(s);
                s = query.get_header();
                
            }
        } else {
            client->write();
        }
    }
    
    void delete_client() {
        delete client;
    }
    
    ~client_handler() = default;
    
private:    
    events_queue* queue;
    struct client* client;
};

struct server_handler : handler {
    server_handler(events_queue* new_queue) :
        queue(new_queue)
    {}
    
    server_handler(server_handler const& other) :
        queue(other.queue),
        server(other.server)
    {}
    
    void handle(struct kevent& event) {
        if (event.flags & EV_EOF && event.data == 0) {
            queue->invalidate_events(server->get_fd());
            server->send_msg();
            delete_server();
            return;
        }
        
    }
    
    void delete_server() {
        delete server;
    }
    
    ~server_handler() = default;
    
private:
    events_queue* queue;
    struct server* server;
};

struct client_listener : handler {
    
    client_listener(events_queue* new_queue) :
        queue(new_queue)
    {}
    
    client_listener(client_listener const& other) :
        queue(other.queue)
    {}
    
    void handle(struct kevent& event) {
        sockaddr_in addr;
        socklen_t length = sizeof(addr);
        int fd = accept(static_cast<int>(event.ident), (sockaddr*) &addr, &length);
        std::cout << "\nClient connected: " << fd << "\n";
        queue->add_event(fd, EVFILT_READ, EV_ADD, nullptr); //TODO: put pointer to event
    }
private:
    events_queue* queue;
};

#endif /* handlers_h */