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

struct http_request {
    
    http_request(std::string const& str);
    
    std::string get_host() const noexcept;
    
    std::string get_header() const noexcept;
    
    ~http_request();
    
private:
    std::string host, header;
    
    void parse_request();
    void parse_first_line();
};

#endif /* http_hpp */
