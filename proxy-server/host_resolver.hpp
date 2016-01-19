//
//  host_resolver.hpp
//  proxy-server
//
//  Created by Дмитрий Новик on 06.01.16.
//  Copyright © 2016 Дмитрий Новик. All rights reserved.
//

#ifndef host_resolver_hpp
#define host_resolver_hpp
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <vector>

#include "http.hpp"
#include "lru_cache.hpp"

struct host_resolver {
    host_resolver(const host_resolver&) = delete;
    host_resolver& operator=(host_resolver const&) = delete;
    
    host_resolver(bool&);
    
    void set_fd(int) noexcept;
    int get_fd() const noexcept;
    
    void push(http_request*);
    http_request* pop();
    
    std::mutex& get_mutex() noexcept;
    void notify();
    void stop();
    
    ~host_resolver();
    
private:
    void write_to_pipe() noexcept;
    void resolve();

/*********** FIELDS ***********/
    int pipe_fd;
    bool& work;
    std::mutex blocker;
    std::condition_variable cv;
    std::queue<http_request*> pending, answers;
    lru_cache<std::string, sockaddr> hosts;
    std::vector<std::thread> threads;
};

#endif /* host_resolver_hpp */
