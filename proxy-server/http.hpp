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
        header(str)
    {
        parse_request();
    }
    
    std::string get_host() {
        return host;
    }
    
    std::string get_header() {
        return header;
    }
    
    ~request() {}
    
private:
    std::string host, header;
    
    void parse_request() {
        size_t i = header.find("Host:");
        if (i == std::string::npos) {
            std::cout << "\nBAD REQUEST FOUND\n";
            return;
        }
        i += 6;
        size_t j = header.find("\r\n", i);
        host = header.substr(i, j - i);
        i = text.find("Connection:");
        i += 12;
        std::cout << "Host for request: " << host << "\n";
    }
    
    void parse_first_line() {
        size_t i = header.find("\r\n\r\n");
        i += 4;
        
        std::string first_line = std::string{header.cbegin(), header.cbegin() + i};
        header = header.substr(i);
        
        i = first_line.find(" ");
        i += 1;
        std::string begin = {first_line.cbegin(), i};
        
        i = first_line.find(host, i);
        i += host.length();
        begin += first_line.substr(i);
        header = begin + header;
    }
};

#endif /* http_h */
