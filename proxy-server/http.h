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
    
    request(char* query)
    {
        //I don't know how to save it in good way
        //TODO: solve this bug
        strcpy(text, query);
        parse_request();
    }
    
    char* get_host() {
        return host;
    }
    
    char* get_text() {
        return text;
    }
    
    ~request() {
        delete [] text;
        delete [] host;
    }
    
private:
    char* host;
    char* text;
    
    void parse_request() {
        char* token;
        token = strtok(text, " ");
        token = strtok(nullptr, " ");
        host = token;
    }
};

#endif /* http_h */
