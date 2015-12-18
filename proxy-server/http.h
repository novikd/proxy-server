//
//  http.h
//  proxy-server
//
//  Created by Дмитрий Новик on 19.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef http_h
#define http_h

struct request {
    
    request(std::string const& str) :
        text(str)
    {
        parse_request();
    }
    
    std::string get_host() {
        return host;
    }
    
    std::string get_text() {
        return text;
    }
    
    ~request() {}
    
private:
    std::string host;
    std::string text;
    
    void parse_request() {
        size_t i = text.find("Host: ");
        if (i == std::string::npos) {
            perror("\nHOST NOT FOUND!\n");
        }
        i += 6;
        size_t j = text.find("\r\n", i);
        host = text.substr(i, j - i);
        std::cout << "\nHost found: " << host << "\n";
    }
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

#endif /* http_h */
