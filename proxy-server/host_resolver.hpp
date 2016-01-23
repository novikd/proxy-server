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
#include "sockets.hpp"

struct host_resolver {
    host_resolver(const host_resolver&) = delete;
    host_resolver& operator=(host_resolver const&) = delete;
    
    host_resolver();
    
    void set_fd(int) noexcept;
    int get_fd() const noexcept;
    
    void push(std::unique_ptr<http_request>);
    std::unique_ptr<http_request> pop();

    void cancel(http_request*);
    void stop();
    
    ~host_resolver();
    
private:
    void write_to_pipe() noexcept;
    void resolve();

/*********** FIELDS ***********/
    file_descriptor pipe_fd;
    bool work;
    std::mutex blocker;
    std::condition_variable cv;
    std::queue<std::unique_ptr<http_request>> pending, answers;
    lru_cache<std::string, sockaddr> hosts;
    std::vector<std::thread> threads;
};

#endif /* host_resolver_hpp */
