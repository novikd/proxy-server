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
    
    void add_etag(std::string const&);
    
    std::string get_host() const noexcept;
    std::string get_header() const noexcept;
    
    static bool check_request_end(std::string const&);
    static bool is_already_cached(std::string const&);
    
    void set_client(int) noexcept;
    int get_client() const noexcept;
    void set_server(sockaddr) noexcept;
    sockaddr get_server() const noexcept;
    
    void cancel() noexcept;
    void set_error() noexcept;
    bool is_canceled() const noexcept;
    bool is_error() const noexcept;
    
    ~http_request();
    
private:
    std::string& header;
    std::string host;
    int client_id;
    sockaddr server_addr;
    bool canceled, error;
    
    void parse_request();
    void parse_first_line();
};

struct http_response {
    
    http_response();
    http_response(bool);
    http_response(http_response const&);
    
    http_response& operator=(http_response const&);
    
    bool check_header_end() const;
    void parse_header();
    bool is_result_304() const;
    
    void set_request(std::string const&);
    
    void delete_request() noexcept;
    std::string& get_request() noexcept;
    
    void force_append_text(std::string const&);
    void append_text(std::string const&);
    std::string& get_text() noexcept;
    void clear_text();
    
    std::string& get_etag() noexcept;
    bool& is_cachable() noexcept; //TODO: rename it
    bool& checking() noexcept;
    
    ~http_response();

private:
    std::string text, ETag, request;
    /*
    / cached == True, if request can be cached
    / check  == True, if we are checking current version
    */
    bool cached, check;
};

#endif /* http_hpp */
