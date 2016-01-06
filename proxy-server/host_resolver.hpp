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
    
    host_resolver(int, bool&);
    
    void push(http_request*);
    http_request* pop();
    void notify();
    
    void stop() noexcept;
    
    ~host_resolver();
    
private:
    void write_to_pipe() const;
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
