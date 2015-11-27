//
//  networking.h
//  proxy-server
//
//  Created by Дмитрий Новик on 27.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef networking_h
#define networking_h

#include "event_queue.h"



struct event_listener {
    
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
    
    static void connecter(struct kevent& event) {
        
    }
    
    struct client {
        
        void execute(struct kevent& event) {
            if (event.filter == event.filter) {
                
            }
        }
        
        char *host, *request;
        int fd;
    };
    
    struct server {
        
        void execute(struct kevent event) {
            
            
        }
    };
};


#endif /* networking_h */
