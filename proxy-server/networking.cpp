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

int init_socket(int port) {
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    int binded = bind(sock, (sockaddr*) &addr, sizeof(addr));
    
    if (binded == -1) {
        perror("Binding arror occured!\n");
    }
    
    listen(sock, SOMAXCONN);
    
    return sock;
}

//This method will be run in another thread to make resolving non-blocking
//TODO: implement it, make it non-blockingю May be I should make it an anonymous function?
void resolve_dns(char* name) {
    struct hostent* addr;
    if ((addr = gethostbyname(name)) == nullptr) {
        perror("Invalid link!\n");
    }
    char* link = addr->h_addr;
}