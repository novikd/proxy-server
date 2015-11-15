//
//  http.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 15.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <string.h>

//this method makes request ready for call
char* parse_http_request(char* request) {
    char* tokens = strtok(request, " \r\n");
    char* result = new char[0];
    
    while (tokens) {
        strcat(result, tokens);
        strcat(result, " ");
        
        if (strcmp(tokens, "GET") == 0) {
            tokens = strtok(nullptr, " \r\n");
            strcat(result, tokens);
            strcat(result, " ");
            tokens = strtok(nullptr, " \r\n");
            strcat(result, tokens);
            strcat(result, "\r\n");
        } else if (strcmp(tokens, "Connection:") == 0) {
            tokens = strtok(nullptr, " \r\n");
            strcat(result, "close\r\n");
        } else {
            tokens = strtok(nullptr, " \r\n");
            strcat(result, tokens);
            strcat(result, "\r\n");
        }
        tokens = strtok(nullptr, " \r\n");
    }
    
    return result;
}

//TODO: Do we need to parse other requests like POST and so on