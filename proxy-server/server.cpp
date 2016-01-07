//
//  server.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 18.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include "networking.hpp"

const unsigned short PORT = 2539;

int main() {

    proxy_server proxy(PORT);
    
    // TODO: in case of exception we should notify threads that they must quit and join them
    
    proxy.run();
    
    return 0;
}