//
//  test.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 04.01.16.
//  Copyright © 2016 Дмитрий Новик. All rights reserved.
//

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <iostream>

int main() {
    int fds[2];
    for (int i = 0; i < 2; ++i)
        fds[i] = socket(PF_LOCAL, SOCK_STREAM, 0);
    
    if (pipe(fds) == -1) {
        perror("Creating a pipe error occurred!");
    }
    
    while (true) {
        std::string s;
        std::cin >> s;
        write(fds[1], s.data(), 20);
        std::vector<char> str(20);
        read(fds[0], str.data(), 20);
        std::cout << str.data() << std::endl;
    }
    
    close(fds[0]);
    close(fds[1]);
}