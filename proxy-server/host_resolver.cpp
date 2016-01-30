//
//  host_resolver.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 06.01.16.
//  Copyright © 2016 Дмитрий Новик. All rights reserved.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cassert>
#include <fcntl.h>
#include <csignal>

#include "exceptions.hpp"
#include "host_resolver.hpp"

host_resolver::host_resolver() :
    work(true)
{
    for (int i = 0; i < 4; ++i) {
        threads.push_back(std::thread([this]() {
            this->resolve();
        }));
    }
}

void host_resolver::set_fd(int fd) noexcept {
    pipe_fd.set_fd(fd);
}

int host_resolver::get_fd() const noexcept {
    return pipe_fd.get_fd();
}

void host_resolver::push(std::unique_ptr<http_request> request) {
    if (!work) {
        throw custom_exception("Resolver has been already stopped!");
    }
    std::unique_lock<std::mutex> locker{blocker};
    pending.push(std::move(request));
    cv.notify_one();
}

std::unique_ptr<http_request> host_resolver::pop() {
    std::unique_lock<std::mutex> locker{blocker};
    std::unique_ptr<http_request> request = std::move(answers.front());
    answers.pop();
    return request;
}

void host_resolver::cancel(http_request* request) {
    if (!work) {
        throw custom_exception("Resolver has been already stopped!");
    }
    std::unique_lock<std::mutex> locker{blocker};
    request->cancel();
}

void host_resolver::stop() {
    std::unique_lock<std::mutex> locker{blocker};
    work = false;
    cv.notify_all();
    locker.unlock();

    for (int i = 0; i < 4; ++i)
        if (threads[i].joinable())
            threads[i].join();
}

host_resolver::~host_resolver() {
    stop();
}

void host_resolver::write_to_pipe() noexcept {
    char tmp = 1;
    std::unique_lock<std::mutex> locker{blocker};
    if (write(pipe_fd.get_fd(), &tmp, sizeof(tmp)) == -1) {
        perror("Sending message to main thread error occurred!\n");
        std::abort();
        // TODO: fix deadlock and joining to itself
        // TODO: probably we can abort here, but we probably (?)
        // should handle the case when pipe is full
        
        // I will ignore it.
    }
}

void host_resolver::resolve() {
    while (work) {
        std::unique_lock<std::mutex> locker(blocker);
        cv.wait(locker, [&]() {
            return !pending.empty() || !work;
        });
        
        if (!work)
            break;
        
        std::cout << "START TO RESOLVE: " << std::this_thread::get_id() << "\n";
        auto request = std::move(pending.front());
        pending.pop();
        
        sockaddr result;
        
        if (hosts.exists(request->get_host())) {
            result = hosts[request->get_host()];
            locker.unlock();
        } else {
            locker.unlock();
            addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_NUMERICSERV;
            
            size_t i = request->get_host().find(":");
            std::string name(request->get_host().substr(0, i)), port = "80";
            
            if (i != std::string::npos)
                port = request->get_host().substr(i + 1);
            
            int error = getaddrinfo(name.c_str(), port.c_str(), &hints, &res);
            
            if (error) {
                perror("\nResolving error occurred!\n");
                locker.lock();
                request->set_error();
                locker.unlock();
            } else {
                result = *res->ai_addr;
                freeaddrinfo(res);
                locker.lock();
                hosts.insert(request->get_host(), result);
                locker.unlock();
            }
        }
        request->set_server(result);
        
        std::cout << "RESOLVED!\n";
        locker.lock();
        answers.push(std::move(request));
        locker.unlock();
        
        write_to_pipe();
    }
}
