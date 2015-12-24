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

//in_addr helper
struct ipv4_adress {
    
    ipv4_adress() :addr() {}
    
    ipv4_adress(ipv4_adress const& other) {
        addr = other.addr;
    }
    
    void set_addr(char* link) {
        inet_aton(link, &addr);
    }
    
    in_addr get_addr() {
        return addr;
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