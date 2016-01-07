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
    header(str),
    canceled(false),
    error(false)
{
    parse_request();
    parse_first_line();
}

http_request::http_request(http_request const& rhs) :
    header(rhs.header),
    host(rhs.host),
    client_id(rhs.client_id),
    server_addr(rhs.server_addr),
    canceled(false),
    error(false)
{}

http_request::http_request(http_request&& rhs) :
    header(rhs.header),
    host(std::move(rhs.host)),
    client_id(rhs.client_id),
    server_addr(rhs.server_addr),
    canceled(false),
    error(false)
{}

std::string http_request::get_host() const noexcept {
    return host;
}

std::string http_request::get_header() const noexcept {
    return header;
}

bool http_request::check_request_end(std::string const& msg) {
    size_t i = msg.find("\r\n\r\n");
    
    if (i == std::string::npos)
        return false;
    if (msg.find("POST") == std::string::npos)
        return true;
    
    size_t j = msg.find("Content-Length: ");
    j += 16;
    size_t content_length = 0;
    while (msg[j] != '\r') {
        content_length *= 10;
        content_length += (msg[j++] - '0');
    }
    
    i += 4;
    return msg.substr(i).length() == content_length;
}

void http_request::set_client(int id) noexcept {
    client_id = id;
}

int http_request::get_client() const noexcept {
    return client_id;
}

void http_request::set_server(sockaddr addr) noexcept {
    server_addr = addr;
}

sockaddr http_request::get_server() const noexcept {
    return server_addr;
}

void http_request::cancel() noexcept {
    canceled = true;
}

void http_request::set_error() noexcept {
    error = true;
}

bool http_request::is_canceled() const noexcept {
    return canceled;
}

bool http_request::is_error() const noexcept {
    return error;
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