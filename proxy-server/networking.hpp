//
//  networking.h
//  proxy-server
//
//  Created by Дмитрий Новик on 27.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef networking_hpp
#define networking_hpp

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

//add to handlers pointer to events_queue
//create new big parant for handlres, which destroys handler and deleting information
//create socket class to create new file descriptor in constructor and close it in destructor
//create events_queue in main() and then send it to proxy constructor
//delet this works
//handler all the errors

//sigterm should stop main_loop and destructors will do everything

#endif /* networking_h */
