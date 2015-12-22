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
#include <fcntl.h>

#include "networking.hpp"
#include "handlers.hpp"
#include "http.h"

const unsigned short PORT = 2539;

bool main_thead;
int main_socket;

void exitor(int param) {
    main_thead = false;
    close(main_socket);
}

int init_socket(int);

int main() {

    signal(SIGINT, exitor);
    
    main_socket = init_socket(PORT);
    int res = fcntl(main_socket, F_SETFL, O_NONBLOCK);
    if (res == -1)
        perror("Making socket non-blocking error occured!\n");
    
    main_thead = true;
    
    events_queue queue;
    proxy proxy_server(main_socket, queue);
    
    std::cerr << "Server started\n";
    
    proxy_server.run();
    
    return 0;
}