//
//  networking.h
//  proxy-server
//
//  Created by Дмитрий Новик on 27.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef networking_hpp
#define networking_hpp

#include <queue>

#include "handlers.hpp"

struct proxy {
    
    proxy(int sock, events_queue& queue) :
        queue(queue),
        main_socket(sock)
    {
        queue.add_event(main_socket, EVFILT_READ, EV_ADD, 0, 0, new struct connector(&queue));
    }
    
    void run() {
        while(true) {
            int amount = queue.occured();
            
            if (amount == -1) {
                perror("Getting new events error occured!\n");
            }
            queue.execute();
        }
    }
    
    
private:
    events_queue queue;
    int main_socket;
};

void resolver(events_queue& queue, std::mutex& global_mutex, std::queue<std::string>& host_queue, std::condition_variable cv) {
    std::unique_lock<std::mutex> locker(global_mutex);
    cv.wait(locker, [&]() {
        return host_queue.size() > 0;
    });
    
    std::string host_name = host_queue.front();
    host_queue.pop();
    
    size_t i = host_name.find(':');
    std::string name, port;
    if (i == std::string::npos) {
        name = host_name;
    } else {
        name = host_name.substr(0, i);
        port = host_name.substr(i);
    }
    
    addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    int error = getaddrinfo(name.c_str(), port.c_str(), &hints, &result);
    
    if (error) {
        perror("\nRESOLVING ERROR OCCURED!\n");
    }
    
    int sock = -1;
    for (addrinfo* tmp = result; tmp; tmp = result->ai_next) {
        sock = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
        
        if (connect(sock, tmp->ai_addr, tmp->ai_addrlen) == -1) {
            perror("\nCAN NOT CONNECT TO THE SERVER!\n");
            close(sock);
            sock = - 1;
            continue;
        }
        break;
    }
    
    if (sock == -1) {
        perror("\nINVALID HOST FOUND!\n");
    }
    freeaddrinfo(result);
    
    //TODO: return result
}

//add to handlers pointer to events_queue +
//create new big parant for handlres, which destroys handler and deleting information +-
//create socket class to create new file descriptor in constructor and close it in destructor +
//create events_queue in main() and then send it to proxy constructor +
//delet this works
//handler all the errors

//sigterm should stop main_loop and destructors will do everything

#endif /* networking_h */
