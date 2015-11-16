//
//  helper.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 15.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

//TODO: To understand what to do with this two classes

//in_addr helper
struct ipv4_adress {
    
    ipv4_adress() :addr() {}
    
    ipv4_adress(ipv4_adress const& other) {
        addr = other.addr;
    }
    
    void set_addr(char* link) {
        inet_aton(link, &addr);
    }
    
    in_addr addr;
};

//where should I use it???
struct ipv4_endpoint {
    
    ipv4_endpoint() : adress(), port() {}
    
    ipv4_endpoint(ipv4_endpoint const& other) {
        adress = other.adress;
        port = other.port;
    }
    
    ipv4_endpoint& operator=(ipv4_endpoint const& other) {
        ipv4_endpoint temp(other);
        return *this;
    }
    
    ipv4_adress adress;
    uint16_t port;
};

//This method will be run in another thread to make resolving non-blocking
//TODO: implement it, make it non-blocking
void resolve_dns(char* name) {
    struct hostent* addr;
    if ((addr = gethostbyname(name)) == nullptr) {
        perror("Invalid link!\n");
    }
    char* link = addr->h_addr;
}

struct events_queue {
    
    events_queue() :
        kq()
    {
        
    }
    
    //this method adds to kqueue event, which listens to other events
    //may be it should also get predicate to be called and std::function, which should be executeded ? Do we need it's identificator?
    void add_listener(struct kevent kev);
    
    void add_event(struct kevent kev);
    
    bool event_occured() {
        return kevent(kq, nullptr, 0, event_list, SOMAXCONN, nullptr) != 0;
    }
    
    void execute() {
        int amount = kevent(kq, nullptr, 0, event_list, SOMAXCONN, nullptr);
        
        if (amount == -1) {
            perror("Getting number of events error!\n");
        }
        
        //TODO: To call event's functions
    }
    
    int get_flags(int i) {
        return event_list[i].flags;
    }
    
    uintptr_t get_ident(int i) {
        return event_list[i].ident;
    }
    
    //TODO: Save all functions and their identificators
    
private:
    int kq;
    struct kevent event_list[SOMAXCONN];
};