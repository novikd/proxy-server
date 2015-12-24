//
//  server.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 18.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <stdio.h>
#include <iostream>
#include <netdb.h>

#include <signal.h>
#include <thread>
#include <functional>

#include "networking.hpp"
#include "handlers.hpp"
#include "http.h"

const unsigned short PORT = 2539;

bool main_thead;
int main_socket;

void exitor(int param) {
    close(main_socket);
}

int init_socket(int);

int main() {

    signal(SIGINT, exitor);
    
    main_socket = init_socket(PORT);
    int res = fcntl(main_socket, F_SETFL, O_NONBLOCK);
    if (res == -1)
        perror("Making socket non-blocking error occured!\n");
    
    proxy_server proxy(main_socket);
    std::vector<std::thread> threads;
    
    for (size_t i = 0; i < 4; ++i)
        threads.push_back(std::thread(resolve_hosts, std::ref(proxy)));
    
    std::cerr << "Server started\n";
    
    proxy.run();
    
    return 0;
}