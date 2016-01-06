//
//  helper.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 15.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <arpa/inet.h>
#include <signal.h>
#include <thread>
#include <cassert>

#include "networking.hpp"

proxy_server::proxy_server(int port) :
    main_socket(init_socket(port)),
    queue(),
    work(true)
{
    for (int i = 0; i < 2; ++i)
        pipe_fds[i] = socket(PF_LOCAL, SOCK_STREAM, 0);
    
    if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("Making pipe-socket #0 non-blocking error occurred!\n");
    }
    if (fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK) == -1) {
        perror("Making pipe-socket #1 non-blocking error occurred!\n");
    }
    
    /*
    / Pipe will help to get messages from resolver's thread
    / I had to use pipe, because EVFILT_USER never triggers
    / @param pipe_fds[0] -- this socket is used for listening to(?) messages
    / @param pipe_fds[1] -- this socket is used for sending msg
    */
    if (pipe(pipe_fds) == -1)
        perror("CREATING PIPE ERROR OCCURRED!\n");
    
    /* Event for connecting new clients */
    queue.add_event(main_socket, EVFILT_READ, EV_ADD, [this](struct kevent& kev) {
        this->connect_client(kev);
    });
    
    /* Event for getting messages from other threads */
    queue.add_event(pipe_fds[0], EVFILT_READ, EV_ADD, [this](struct kevent& kev) {
        this->on_host_resolved(kev);
    });
    
    /* Event for signals handling */
    queue.add_event(SIGINT, EVFILT_SIGNAL, EV_ADD, [this](struct kevent& event) {
        std::cout << "SIGINT CAUGHT!";
        this->hard_stop();
    });
}

void proxy_server::run() {
    std::cout << "Server started\n";
    
    try {
        while (work) {
            int amount = queue.occured();
            
            if (amount == -1) {
                perror("Getting new events error occurred!\n");
            }
            queue.execute();
        }
    } catch(...) {
        //It stops the server and resolvers
        hard_stop();
    }
}

void proxy_server::start() {
    work = true;
}

void proxy_server::hard_stop() {
    work = false;
}

void proxy_server::stop() {
    queue.delete_event(main_socket, EVFILT_READ);
}

proxy_server::~proxy_server() {
    queue.delete_event(SIGINT, EVFILT_SIGNAL);
    queue.delete_event(pipe_fds[0], EVFILT_READ);
    
    for (auto client : clients) {
        delete client.second;
    }
    //All the servers must be deleted in that moment
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    close(main_socket);
}

int proxy_server::init_socket(int port) {
    int main_socket = socket(PF_INET, SOCK_STREAM, 0);
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(main_socket, (sockaddr*) &addr, sizeof(addr)) == -1) {
        perror("Binding error occured!\n");
    }

    if (fcntl(main_socket, F_SETFL, O_NONBLOCK) == -1) {
        perror("Making socket non-blocking error occurred!\n");
    }
    
    listen(main_socket, SOMAXCONN);

    return main_socket;
}

void proxy_server::disconnect_client(struct kevent& event) {
    std::cout << "Disconnect client: " << event.ident << "\n";
    struct client* client = clients[event.ident];
    
    auto it = requests.find(client);
    if (it != requests.end()) {
        it->second->cancel();
        requests.erase(client);
    }
    
    if (client->has_server())
        queue.invalidate_events(client->get_server_fd());
    
    queue.invalidate_events(client->get_fd());
    clients.erase(event.ident);
    queue.delete_event(event.ident, EVFILT_WRITE);
    queue.delete_event(event.ident, EVFILT_READ);
    
    if (client->has_server()) {
        int server_fd = client->get_server_fd();
        servers.erase(server_fd);
        queue.delete_event(server_fd, EVFILT_READ);
        queue.delete_event(server_fd, EVFILT_WRITE);
    }
    delete client;
}

void proxy_server::disconnect_server(struct kevent& event) {
    std::cout << "Disconnect server: " << event.ident << "\n";
    struct server* server = servers[event.ident];
    
    queue.invalidate_events(event.ident);
    servers.erase(event.ident);
    queue.delete_event(event.ident, event.filter);
    
    delete server;
}

void proxy_server::connect_client(struct kevent& event) {
    /*
    / If new throw the exception here proxy server must stop
    / Exception will be caught in the function run()
    */
    client* new_client = new client(main_socket);
    clients[new_client->get_fd()] = new_client;
    queue.add_event(new_client->get_fd(), EVFILT_READ, EV_ADD, [this](struct kevent& ev) {
        this->read_from_client(ev);
    });
}

//TODO: To finish realization
void proxy_server::read_from_client(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_client(event);
        return;
    }
    
    std::cout << "Reading from client: " << event.ident << "\n";
    struct client* client = clients.at(event.ident);
    client->read(event.data);
    
    //???: Is this line necessary now?
    queue.delete_event(event.ident, EVFILT_WRITE);
    if (client->check_request_end()) {
        http_request* request = new http_request(client->get_buffer());
        request->set_client(client->get_fd());
        requests[client] = request;
        
        std::cout << client->get_buffer();

        std::unique_lock<std::mutex> locker(blocker);
        pending.push(request);
        locker.unlock();

        cv.notify_one();
    }
}

void proxy_server::read_from_server(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }
    
    std::cout << "Reading from server: " << event.ident << "!\n";
    struct server* server = servers[event.ident];
    size_t size = server->read(event.data);
    if (size > 0) {
        server->send_msg();
        queue.add_event(server->get_client_fd(), EVFILT_WRITE, EV_ENABLE, 0, 0, nullptr);
    }
}

void proxy_server::write_to_client(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_client(event);
        return;
    }
    
    std::cout << "Writing to client: " << event.ident << "!\n";
    struct client* client = clients[event.ident];
    client->write();
    if (client->has_server())
        client->get_msg();
    if (client->get_buffer_size() == 0)
        queue.add_event(event.ident, event.filter, EV_DISABLE, 0, 0, nullptr);
}

void proxy_server::write_to_server(struct kevent& event) {
    std::cout << "Writing to server: " << event.ident << "\n";
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }
    
    int err;
    socklen_t len = sizeof(err);

    if (getsockopt(static_cast<int>(event.ident), SOL_SOCKET, SO_ERROR, &err, &len) == -1 || err != 0) {
        //???: To stop proxy-server at that moment?
        disconnect_server(event);
        perror("Connecting to server failed!");
        return;
    }
    
    struct server* server = servers[event.ident];
    //TODO: may be server should have capacity?
    server->write();
    if (server->buffer_empty()) {
        queue.add_event(event.ident, EVFILT_READ, EV_ADD, [this](struct kevent& kev) {
            this->read_from_server(kev);
        });
        queue.delete_event(event.ident, event.filter);
        queue.add_event(server->get_client_fd(), EVFILT_WRITE, EV_ADD, [this](struct kevent& kev) {
            this->write_to_client(kev);
        });
    }
}

void proxy_server::on_host_resolved(struct kevent& event) {
    std::cout << "ON_HOST_RESOLVED" << "\n";
    std::vector<char> tmp(1);
    if (read(pipe_fds[0], tmp.data(), sizeof(char)) == -1) {
        perror("GETTING MESSAGE FROM ANOTHER THREAD ERROR OCCURRED!\n");
    }
    
    std::unique_lock<std::mutex> locker(blocker);
    http_request* request{std::move(answers.front())};
    answers.pop();
    locker.unlock();
    std::cout <<"Client: " << request->get_client() << "\n";
    
    if (request->is_canceled())
        return;
    
    //TODO: check that client is valid and handle cancelation
    struct client* client = clients.at(request->get_client());
    requests.erase(client);
    if (!client->has_server()) {
        sockaddr result = std::move(request->get_server());
        int server_socket = -1;
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            perror("\nGETTING SOCKET ERROR OCCURRED\n");
        }
        
        const int set = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) {
            perror("\nSETTING SOCKET ERROR OCCURRED!\n");
        };
        
        if (fcntl(server_socket, F_SETFL, O_NONBLOCK) == -1) {
            perror("\nMAKING SOCKET NON-BLOCKING ERROR OCCURRED\n");
        }
        
        std::cout << "Connecting to server: " << request->get_host() << "\nIP: "<< inet_ntoa(((sockaddr_in*) &result)->sin_addr) << "\nSocket: " << server_socket << "\n";
        if (connect(server_socket, &result, sizeof(result)) == -1) {
            if (errno != EINPROGRESS) {
                //TODO: stop proxy-server here
                perror("\nCONNECTING ERROR OCCURRED\n");
                close(server_socket);
                server_socket = -1;
            }
        }

        try {
            struct server* server = new struct server(server_socket);
            client->bind(server);
            server->bind(client);
            servers[server->get_fd()] = server;
            queue.add_event(server_socket, EVFILT_WRITE, EV_ADD, [this](struct kevent& kev) {
                this->write_to_server(kev);
            });
        } catch (...) {
            //???: stop the server here?
            perror("\nCREATING NEW SERVER ERROR OCCURRED!\n");
        }
    } else {
        queue.add_event(client->get_server_fd(), EVFILT_WRITE, EV_ADD, [this](struct kevent& kev){
            this->write_to_server(kev);
        });
    }
    client->send_msg();
}

void proxy_server::resolve_hosts() {
    while (work) {
        std::unique_lock<std::mutex> locker(blocker);
        cv.wait(locker, [&]() {
            return !pending.empty();
        });
        std::cout << "START TO RESOLVE: " << std::this_thread::get_id() << "\n";
        auto request = pending.front();
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
                perror("\nRESOLVING ERROR OCCURRED\n");
                assert(false);
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
        answers.push(request);
        locker.unlock();
        
        char msg = 1;
        if (write(pipe_fds[1], &msg, sizeof(msg)) == -1) {
            hard_stop();
        }
    }
}