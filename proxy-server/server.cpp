//
//  server.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 18.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <stdio.h>
#include <iostream>

#include <signal.h>
#include <fcntl.h>
#include "event_queue.h"

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
        struct kevent event;
        EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        if (queue.add_event(event) != -1) {
            close(fd);
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
        char* message = new char[0];
        int fd = static_cast<int>(kev.ident);
        ptrdiff_t size = 1;
        while (size > 0) {
            size = recv(fd, buffer, BUFFER_SIZE, 0);
            if (size > 0)
                strncat(message, buffer, size);
            //bzero(buffer, BUFFER_SIZE);
        }
        printf("%s\n", message);
        delete[] message;
        delete[] buffer;
    }
}

int main() {

    signal(SIGTERM, exitor);
    
    //ask about non-blocking, select() and may be it is possible to use kqueue
    main_socket = init_socket(PORT);
    fcntl(main_socket, F_SETFL, O_NONBLOCK);
    main_thead = true;
    
    std::cerr << "Server started\n";
    
    struct kevent kev;
    EV_SET(&kev, main_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    queue.add_listener(kev, std::function<bool(struct kevent)>(is_connection), std::function<void(struct kevent)>(connection));
    
    while (main_thead) {
        if (queue.event_occured()) {
            std::cerr << "EVENT\n\n\n";
            queue.execute();
        }
    }
    
    return 0;
}