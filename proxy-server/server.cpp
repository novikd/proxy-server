//
//  server.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 18.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

//#include <stdio.h>
//#include <iostream>
//#include <netdb.h>
//

#include <thread>
#include "networking.hpp"

const unsigned short PORT = 2539;

//void resolve_hosts(proxy_server&);

int main() {

    proxy_server proxy(PORT);
//    std::vector<std::thread> threads;
//    
////    for (size_t i = 0; i < 4; ++i)
////        threads.push_back(std::thread(resolve_hosts, std::ref(proxy)));
//    
//    // TODO: in case of exception we should notify threads that they must quit and join them
    
    proxy.run();

    
    return 0;
}