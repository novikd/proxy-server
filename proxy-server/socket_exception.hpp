//
//  socket_exception.hpp
//  proxy-server
//
//  Created by Дмитрий Новик on 26.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef socket_exception_hpp
#define socket_exception_hpp

#include <exception>
#include <iostream>
//TODO: rename this class and reuse it in all parts of programm
struct socket_exception : std::exception {
    socket_exception(const std::string& str) :
    error_info(str)
    {}
    
    socket_exception(std::string&& msg) :
    error_info(std::move(msg))
    {}
    
    const char* what() const noexcept override {
        return error_info.c_str();
    }
    
private:
    std::string error_info;
};

#endif /* socket_exception_hpp */
