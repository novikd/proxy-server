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
#include "tcp.hpp"

struct connection_handler : handler {
    
};
//TODO: to write a lambda functions. I don't know where is error.
//But they look really strange
struct client : handler
{
    client(events_queue* queue, tcp_connection* connection) :
        queue(queue),
        connection(connection)
    {}
    
    void handle(struct kevent& event) {
        if (event.flags & EV_EOF && event.data == 0) {
            std::cout << "Disconnect!\n";
            int fd = static_cast<int>(event.ident);
            struct handler* handler = (struct handler*) event.udata;
            delete connection;
            delete handler;
            queue->add_event(fd, EVFILT_READ, EV_DELETE, 0, 0, this);
            close(fd);
//        } else if (event.flags & EV_EOF) {
//            parse_host();
//            std::cout << host << "\n";
//            int fd = static_cast<int>(event.ident);
            EV_SET(&event, fd, EVFILT_WRITE, EV_ADD, 0, 0, this);
        } else if (event.filter & EVFILT_READ) {
            std::cout << "Reading!\n";
            char* buffer = new char[event.data];
            int fd = static_cast<int>(event.ident);
            size_t message_size = recv(fd, buffer, event.data, 0);
            if (message_size == -1) {
                perror("Getting message error occured!\n");
            }
            request.append(buffer);
            std::cout << request;
            if (check_request_end()) {
                parse_host();
                in_addr tmp = resolve();
                connection->init_server(tmp);
                queue->add_event(fd, EVFILT_READ, EV_DELETE, 0, 0, this);
                queue->add_event(connection->get_server_fd(), EVFILT_WRITE, EV_ADD, 0, 0, this);
            }
            //queue.add_event(event.ident, EVFILT_READ, EV_ADD, 0, 0, this);
            delete[] buffer;
        }
    }
    
    ~client() {
        delete connection;
    }
    
    std::string host, request;
private:
    events_queue* queue;
    tcp_connection* connection;
    
    void parse_host() {
        size_t i = request.find("Host:");
        i += 6;
        size_t j = request.find("\r\n", i);
        host = request.substr(i, j - i);
        std::cout << "Host for request: " << host << "\n";
    }
    
    bool check_request_end() {
        size_t i;
        if ((i = request.find("\r\n\r\n")) == std::string::npos) {
            return false;
        }
        
        if (request.find("POST") == std::string::npos)
            return true;
        
        i += 4;
        
        if (request.find("\r\n\r\n", i) != std::string::npos)
            return true;
        
        return false;
    }
    
    in_addr resolve() {
        struct hostent* hostent = gethostbyname(host.c_str());
        if (hostent == nullptr) {
            perror("Resolving host error occured!\n");
        }
        
        in_addr addr;
        if (inet_aton(hostent->h_addr, &addr) == 0) {
            std::cout << hostent->h_addr << "\n";
            perror("Invalid IP address!\n");
        }
        
        return addr;
    }
};

struct socket_listener : handler
{
    socket_listener(events_queue* queue) :
        queue(queue)
    {}
    
    void handle(struct kevent& event) {
        std::cout << "Connect!\n";
        int sock = static_cast<int>(event.ident);
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int fd = accept(sock, (sockaddr*) &addr, &len);
        queue->add_event(fd, EVFILT_READ, EV_ADD, 0, 0, new client(queue, new tcp_connection(fd)));
    }
    
    ~socket_listener() {
        
    }
    
private:
    events_queue* queue;
};


#endif /* handlers_h */
