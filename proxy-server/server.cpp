//
//  server.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 18.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include "networking.hpp"
#include <signal.h>

const unsigned short PORT = 2539;

int main() {

    proxy_server proxy(PORT);
    
/*
/ Turned off, because XCode crashes on pressing STOP button
/ It sends SIGINT and after that sends SIGKILL
*/
    proxy.handle_signal(SIGINT, [&proxy](struct kevent& kev) {
        std::cout << "\nSIGINT\n";
        proxy.hard_stop();
    });
    proxy.handle_signal(SIGTERM, [&proxy](struct kevent& kev) {
        std::cout << "\nSIGTERM\n";
        proxy.stop();
    });

    proxy.run();
    
    return 0;
}
