//
//  http.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 27.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <stdio.h>
#include "http.hpp"

/********** HTTP_REQUEST **********/

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

void http_request::add_etag(std::string const& etag) {
    header.resize(header.size() - 2);
    header.append("If-None-Match: " + etag + "\r\n\r\n");
}

std::string http_request::get_host() const noexcept {
    return host;
}

std::string http_request::get_header() const noexcept {
    return header;
}

std::string http_request::get_url() const {
    size_t i = header.find(" ");
    while (header[i] == ' ') {
        i++;
    }
    size_t j = header.find(" ", i);
    return host + header.substr(i, j - i);
}

bool http_request::check_request_end(std::string const& msg) {
    size_t i = msg.find("\r\n\r\n");
    
    if (i == std::string::npos)
        return false;
    if (msg.find("POST") == std::string::npos) // TODO: WAT?
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

bool http_request::is_already_cached(std::string const& request) {
    if (request.find("GET") == std::string::npos)
        return true;
    
    if (request.find("If-Match") != std::string::npos)
        return true;

    if (request.find("If-Modified-Since") != std::string::npos)
        return true;
    
    if (request.find("If-None-Match") != std::string::npos)
        return true;

    if (request.find("If-Range") != std::string::npos)
        return true;

    if (request.find("If-Unmodified-Since") != std::string::npos)
        return true;
    
    return false;
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

/********** HTTP_RESPONSE **********/

http_response::http_response() :
    cached(true),
    check(false)
{}

http_response::http_response(bool is_already_cached) :
    cached(!is_already_cached),
    check(false)
{}

http_response::http_response(http_response const& rhs) :
    text(rhs.text),
    ETag(rhs.ETag),
    request_url(rhs.request_url),
    cached(rhs.cached),
    check(rhs.check)
{}

http_response& http_response::operator=(http_response const& rhs) {
    text = rhs.text;
    ETag = rhs.ETag;
    request_url = rhs.request_url;
    cached = rhs.cached;
    check = rhs.check;
    return *this;
}

bool http_response::check_header_end() const {
    return text.find("\r\n\r\n") != std::string::npos;
}

void http_response::parse_header() {
    {
        size_t i = text.find("Content-Type: ");
        if (i != std::string:: npos) {
            size_t j = text.find("\r", i);
            if (text.substr(i, j - i).find("video") != std::string::npos)
                cached = false;
        }
    }
    size_t i = text.find("ETag: ");
    cached = cached && i != std::string::npos;
    parse_control_line();
    
    if (cached) {
        i += 6;
        size_t j = 0;
        while (text[i + j] != '\r') {
            ++j;
        }
        ETag = text.substr(i, j);
    }
}

bool http_response::is_result_304() const {
    size_t i = text.find("\r\n");
    size_t j = text.substr(0, i).find("304");
    return j != std::string::npos;
}

void http_response::set_request(std::string const& str) {
    request_url = str;
}

std::string& http_response::get_request() noexcept {
    return request_url;
}

void http_response::delete_request() noexcept {
    request_url.clear();
}

void http_response::force_append_text(std::string const& str) {
    text.append(str);
}

void http_response::append_text(std::string const& str) {
    if (cached && !check) {
        text.append(str);
    }
}

std::string& http_response::get_text() noexcept {
    return text;
}

void http_response::clear_text() {
    text.clear();
}

std::string& http_response::get_etag() noexcept {
    return ETag;
}

bool& http_response::is_cachable() noexcept {
    return cached;
}

bool& http_response::checking() noexcept {
    return check;
}

http_response::~http_response() {}

void http_response::parse_control_line() {
    size_t i = text.find("Cache-Control: ");
    if (i == std::string::npos)
        return;
    
    size_t j = text.find("\r", i);
    std::string temp = text.substr(i, j - i);
    cached = cached && (temp.find("private") == std::string::npos) && (temp.find("no-store") == std::string::npos);
}
