//
//  event_queue.h
//  proxy-server
//
//  Created by Дмитрий Новик on 17.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef event_queue_hpp
#define event_queue_hpp

#define EVLIST_SIZE 512

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <functional>
#include <netinet/in.h> // sockaddr_in

//#include "tcp.hpp"

typedef std::function<void(struct kevent&)> funct_t;

struct events_queue;

struct handler {
    virtual void handle(struct kevent& event) {
        
    }
};

//struct tester : handler {
//    void handle(struct kevent& event) {
//        
//    }
//};

//struct events_queue {
//    events_queue();
//    
//    int add_event(struct kevent& event);
//    int add_event(uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data, void* udata);
//    int occured();
//    void execute();
//};

struct events_queue {
    
    events_queue() :
        kq(kqueue())
    {}
    
    //this method adds to kqueue event, which listens to other events
    //may be it should also get predicate to be called and std::function, which should be executeded ? Do we need it's identificator?
    //How should I save arguments!? Map or they must have different identificators?
    //TODO:: Remove std::function from function signature
//    void add_listener(struct kevent kev, std::function<bool(struct kevent)> predicate, std::function<void(struct kevent)> operation) {
//        if (kevent(kq, &kev, 1, nullptr, 0, nullptr) != -1) {
//            predicates.push_back(predicate);
//            operations.push_back(operation);
//        } else {
//            perror("Adding new kevent error");
//        }
//    }
    
//    void add_listener(uintptr_t ident, int16_t filter, std::function<void(struct kevent)> f) {
//        tmp_handlers[std::make_pair(ident, filter)] = f;
//    }
//    
//    void add_listener(holder event_holder, funct_t handler) {
//        handlers[event_holder] = handler;
//    }
    
    //TODO: Create a kevent inside the function! Function should get only params for kevent(...) .
    int add_event(const struct kevent& kev) {
        return kevent(kq, &kev, 1, nullptr, 0, nullptr);
    }
    
    int add_event(uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data, void* udata) {
        struct kevent kev;
        EV_SET(&kev, ident, filter, flags, fflags, data, udata);
        return kevent(kq, &kev, 1, nullptr, 0, nullptr);
    }
    
    int occured() {
        return kevent(kq, nullptr, 0, event_list, EVLIST_SIZE, nullptr);
    }
    
    void execute() {
        int amount = kevent(kq, nullptr, 0, event_list, EVLIST_SIZE, nullptr);
        
        if (amount == -1) {
            perror("Getting number of events error!\n");
        }
        
//        handler* h = static_cast<handler*>(event_list[0].udata);
//        h->handle(event_list[0]);
        for (int i = 0; i < amount; ++i) {
            handler *event_handler = static_cast<handler*> (event_list[i].udata);
            event_handler->handle(event_list[i]);
        }
    }
    
//    void run() {
//        while (true) {
//            int amount = kevent(kq, nullptr, 0, event_list, EVLIST_SIZE, nullptr);
//            
//            if (amount == -1) {
//                perror("Kqueue error! Can't to get amount of events\n");
//            }
//            for (int i = 0; i < amount; ++i) {
//                auto it = handlers.find({event_list[i]});
//                
//                if (it == handlers.end()) {
//                    perror("Can't find handler for event\n");
//                    continue;
//                }
//                
//                it->second(event_list[i]);
//            }
//        }
//    }
    
    //TODO: Save all functions and their identificators
    //Should I use std::function or pointers to functions.
private:
    int kq;
    struct kevent event_list[EVLIST_SIZE];
//    std::map<std::pair<int, int>, std::function<void(struct kevent)> > tmp_handlers;
//    std::map<holder, funct_t> handlers;
//    std::vector<std::function<bool(struct kevent)> > predicates;
//    std::vector<std::function<void(struct kevent)> > operations;
};
#endif /* event_queue_h */
