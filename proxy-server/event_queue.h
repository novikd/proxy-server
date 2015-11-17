//
//  event_queue.h
//  proxy-server
//
//  Created by Дмитрий Новик on 17.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef event_queue_h
#define event_queue_h

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <vector>
#include <functional>
#include <netinet/in.h> // sockaddr_in

struct events_queue {
    
    events_queue() :
    kq(kqueue())
    {
        
    }
    
    //this method adds to kqueue event, which listens to other events
    //may be it should also get predicate to be called and std::function, which should be executeded ? Do we need it's identificator?
    //How should I save arguments!? Map or they must have different identificators?
    void add_listener(struct kevent kev, std::function<bool(struct kevent)> predicate, std::function<void(struct kevent)> operation) {
        if (kevent(kq, &kev, 1, nullptr, 0, nullptr) != -1) {
            predicates.push_back(predicate);
            operations.push_back(operation);
        } else {
            perror("Adding new kevent error");
        }
    }
    
    void add_event(struct kevent kev) {
        if (kevent(kq, &kev, 1, nullptr, 0, nullptr) == -1) {
            perror("Adding new kevent error!\n");
        }
    }
    
    bool event_occured() {
        return kevent(kq, nullptr, 0, event_list, SOMAXCONN, nullptr) != 0;
    }
    
    void execute() {
        int amount = kevent(kq, nullptr, 0, event_list, SOMAXCONN, nullptr);
        
        if (amount == -1) {
            perror("Getting number of events error!\n");
        }
        
        //TODO: To call event's functions.
        //Is it work?
        for (int i = 0; i < amount; ++i) {
            for (size_t j = 0; j < predicates.size(); ++j)
                if (predicates[j](event_list[i])) {
                    operations[j](event_list[i]);
                    break;
                }
        }
    }
    
    int get_flags(int i) {
        return event_list[i].flags;
    }
    
    uintptr_t get_ident(int i) {
        return event_list[i].ident;
    }
    
    int get_queue() {
        return kq;
    }
    
    //TODO: Save all functions and their identificators
    //Should I use std::function or pointers to functions.
private:
    int kq;
    struct kevent event_list[SOMAXCONN];
    std::vector<std::function<bool(struct kevent)> > predicates;
    std::vector<std::function<void(struct kevent)> > operations;
};

#endif /* event_queue_h */
