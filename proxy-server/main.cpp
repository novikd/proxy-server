//
//  main.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 14.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <iostream>
#include "event_queue.h"
#include <signal.h>

char* parse_http_request(char* s);
int init_socket(int);
bool cycle = true;
int main_socket;

void exitor(int param) {
    cycle = false;
    close(main_socket);
}

//this file for testing
int main() {
    
//    char str[] = "GET http://www.site.ru/news.html?login=Petya%20Vasechkin&password=qq HTTP/1.0\r\nHost: www.site.ru\r\nReferer: http://www.site.ru/index.html\r\nCookie: income=1\r\n\r\n";
    
    //char* request = parse_http_request(str);
    
    //std::cout << request;

    main_socket = init_socket(2539);
    
    signal(SIGTERM, *exitor);
    
    events_queue queue;
    
    auto f = [&] (struct kevent event) {
        return event.ident == main_socket || (event.flags & EV_EOF) || (event.flags & EVFILT_READ);
    };
    
    auto g = [&] (struct kevent event) {
        if (event.flags & EV_EOF) {
            int fd = static_cast<int>(event.ident);
            struct kevent kev;
            EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            if (kevent(queue.get_queue(), &kev, 1, nullptr, 0, nullptr) == -1) {
                perror("Deleting error occured!\n");
            }
            close(fd);
        } else if (event.ident == main_socket) {
            sockaddr_in addr;
            socklen_t size = sizeof(addr);
            int fd = accept(static_cast<int>(event.ident), (sockaddr*) &addr, &size);
            
            if (fd == -1) {
                perror("Connection error occured!\n");
            }
            struct kevent kev;
            EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
            kevent(queue.get_queue(), &kev, 1, nullptr, 0, nullptr);
            send(fd, "Welcome!\n", 9, 0);
        } else if (event.flags & EVFILT_READ) {
            int fd = static_cast<int>(event.ident);
            char* buffer = new char[256];
            size_t size = recv(fd, buffer, 256, 0);
            if (size == -1) {
                perror("Getting message error!\n");
            }
            send(fd, buffer, size, 0);
            delete[] buffer;
        }
    };
    
    struct kevent kev;
    EV_SET(&kev, main_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    
    queue.add_listener(kev, std::function<bool(struct kevent)>(f), std::function<void(struct kevent)>(g));
    
    while (cycle) {
        if (queue.event_occured()) {
            queue.execute();
        }
    }
    
    return 0;
}