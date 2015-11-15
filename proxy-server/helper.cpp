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
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

//TODO: To understand what to do

//in_addr helper
struct ipv4_adress {
    
    ipv4_adress(std::string host_name)
    {
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        
        struct hostent* host = gethostbyname(host_name.c_str());
        memcpy(&addr.sin_addr, host->h_addr, host->h_length);
    }
    
    struct sockaddr_in addr;
};

//sockaddr_in helper
struct ipv4_endpoint {
    
    ipv4_endpoint(std::string host_name) :
        adress(host_name)
    {
        
    }
    
    ipv4_adress adress;
    uint16_t port;
};