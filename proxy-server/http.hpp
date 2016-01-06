//
//  http.h
//  proxy-server
//
//  Created by Дмитрий Новик on 19.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef http_hpp
#define http_hpp
#include <iostream>
#include <sys/socket.h>

struct http_request {
    
    http_request(std::string&);
    http_request(http_request const&);
    http_request(http_request&&);
    
    std::string get_host() const noexcept;
    std::string get_header() const noexcept;
    
    void set_client(int) noexcept;
    int get_client() const noexcept;
    void set_server(sockaddr) noexcept;
    sockaddr get_server() const noexcept;
    
    ~http_request();
    
private:
    std::string& header;
    std::string host;
    int client_id;
    sockaddr server_addr;
    
    void parse_request();
    void parse_first_line();
};

#endif /* http_hpp */
