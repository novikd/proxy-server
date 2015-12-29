//
//  http.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 27.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <stdio.h>
#include "http.hpp"

http_request::http_request(std::string& str) :
    header(str)
{
    parse_request();
    parse_first_line();
}

std::string http_request::get_host() const noexcept {
    return host;
}

std::string http_request::get_header() const noexcept {
    return header;
}

http_request::~http_request() {}

void http_request::parse_request() {
    size_t i = header.find("Host:");
    if (i == std::string::npos) {
        std::cout << "\nBAD REQUEST FOUND\n";
        return;
    }
    i += 6;
    size_t j = header.find("\r\n", i);
    host = header.substr(i, j - i);
    i = header.find("Connection:");
    i += 12;
    std::cout << "Host for request: " << host << "\n";
}

void http_request::parse_first_line() {
    size_t i = header.find("\r\n\r\n");
    i += 4;
    
    std::string first_line = std::string{header.cbegin(), header.cbegin() + i};
    header = header.substr(i);
    
    i = first_line.find(" ");
    i += 1;
    std::string begin = std::string{first_line.cbegin(), first_line.cbegin() + i};
    
    i = first_line.find(host, i);
    i += host.length();
    begin += first_line.substr(i);
    header = begin + header;
}
