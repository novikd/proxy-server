//
//  main.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 14.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <iostream>

char* parse_http_request(char* s);
//this file for testing
int main() {
    
    char str[] = "GET http://www.site.ru/news.html?login=Petya%20Vasechkin&password=qq HTTP/1.0\r\nHost: www.site.ru\r\nReferer: http://www.site.ru/index.html\r\nCookie: income=1\r\n\r\n";
    
    char* request = parse_http_request(str);
    
    std::cout << request;
    return 0;
}
