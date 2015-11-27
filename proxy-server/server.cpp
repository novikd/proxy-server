//
//  server.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 18.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <stdio.h>
#include <iostream>
#include <netdb.h>

#include <signal.h>

#include "networking.h"
#include "http.h"

#define PORT 2539
#define BUFFER_SIZE 1024

events_queue queue;
bool main_thead;
int main_socket;

void exitor(int param) {
    main_thead = false;
    close(main_socket);
}

int init_socket(int);

bool is_connection(struct kevent kev) {
    return (kev.flags & EV_EOF) || (kev.ident == main_socket) || (kev.filter & EVFILT_READ);
}

void connection(struct kevent kev) {
    if (kev.flags & EV_EOF) {
        int fd = static_cast<int>(kev.ident);
        char* message = static_cast<char*>(kev.udata);
        if (message) {
            request query(message);
            struct hostent* host;
            if ((host = gethostbyname(query.get_host())) != nullptr) {
                char* temp = host->h_addr;
            }
        } else {
            struct kevent event;
            EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            if (queue.add_event(event) != -1) {
                close(fd);
            }
        }
    } else if (kev.ident == main_socket) {
        sockaddr_in addr;
        socklen_t size = sizeof(addr);
        int fd = accept(main_socket, (sockaddr*) &addr, &size);
        struct kevent event;
        EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        queue.add_event(event);
    } else if (kev.flags & EVFILT_READ) {
        char* buffer = new char[BUFFER_SIZE];
        char* message;
        int fd = static_cast<int>(kev.ident);
        if (kev.udata) {
            message = static_cast<char*>(kev.udata);
        } else {
            message = new char[BUFFER_SIZE];
        }
        recv(fd, buffer, BUFFER_SIZE, 0);
        strncat(message, buffer, strlen(buffer));
        struct kevent event;
        EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 0, message);
        queue.add_event(event);
        delete[] buffer;
    }
}

int main() {

    signal(SIGTERM, exitor);
    
    //ask about non-blocking, select() and may be it is possible to use kqueue
    main_socket = init_socket(PORT);
//    fcntl(main_socket, F_SETFL, O_NONBLOCK);
    main_thead = true;
    
    std::cerr << "Server started\n";
    
    struct kevent kev;
    EV_SET(&kev, main_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    queue.add_listener(kev, std::function<bool(struct kevent)>(is_connection), std::function<void(struct kevent)>(connection));
    
    queue.run();
    
    return 0;
}