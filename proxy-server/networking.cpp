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
    work(true),
    resolver(work)
{
    /*
     / Pipe will help to get messages from resolver's thread
     / I had to use pipe, because EVFILT_USER never triggers
     / @param pipe_fd -- this socket is used for listening to(?) messages
     */

    int fds[2];
    if (pipe(fds) == -1) {
        perror("Creating a pipe error occurred!\n");
    }
    
    if (fcntl(fds[0], F_SETFL, O_NONBLOCK) == -1) {
        hard_stop();
        perror("Making pipe-socket for listening non-blocking error occurred!\n");
    }
    if (fcntl(fds[1], F_SETFL, O_NONBLOCK) == -1) {
        hard_stop();
        perror("Making pipe-socket for writing non-blocking error occurred!\n");
    }
    
    resolver.set_fd(fds[1]);
    pipe_fd = fds[0];
    
    /* Event for connecting new clients */
    queue.add_event(main_socket, EVFILT_READ, EV_ADD, [this](struct kevent& kev) {
        this->connect_client(kev);
    });
    
    /* Event for getting messages from resolver's threads */
    queue.add_event(pipe_fd, EVFILT_READ, EV_ADD, [this](struct kevent& kev) {
        this->on_host_resolved(kev);
    });
    
    /* Event for signals handling */
    signal(SIGINT, SIG_IGN);
    queue.add_event(SIGINT, EVFILT_SIGNAL, EV_ADD, [this](struct kevent& event) {
        std::cout << "SIGINT CAUGHT!\n";
        this->hard_stop();
    });
}

void proxy_server::run() {
    std::cout << "Server started\n";
    
    try {
        while (work) {
            int amount = queue.occured();
            
            if (amount == -1) {
                //???: stop server here?
                perror("Getting new events error occurred!\n");
            }
            queue.execute();
        }
    } catch(...) {
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
    /*
    / Event for connecting is already deleted
    */
    queue.delete_event(SIGINT, EVFILT_SIGNAL);
    queue.delete_event(pipe_fd, EVFILT_READ);
    
    for (auto client : clients) {
        delete client.second;
    }
    /*
    / All the servers must be deleted at that moment
    */
    close(pipe_fd);
    close(main_socket);
    
    std::cout << "Server stoped!\n";
}

int proxy_server::init_socket(int port) {
    int new_socket = socket(PF_INET, SOCK_STREAM, 0);
    
    const int set = 1;
    if (setsockopt(new_socket, SOL_SOCKET, SO_REUSEPORT, &set, sizeof(set)) == -1) {
        perror("\nSetting main socket SO_REUSEPORT error occurred!\n");
    };
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(new_socket, (sockaddr*) &addr, sizeof(addr)) == -1) {
        perror("Binding error occured!\n");
    }

    if (fcntl(new_socket, F_SETFL, O_NONBLOCK) == -1) {
        perror("Making socket non-blocking error occurred!\n");
    }
    
    if (listen(new_socket, SOMAXCONN) == -1) {
        perror("Starting server error occurred!\n");
    }

    return new_socket;
}

void proxy_server::disconnect_client(struct kevent& event) {
    std::cout << "Disconnect client: " << event.ident << "\n";
    struct client* client = clients[event.ident];
    
    {
        std::unique_lock<std::mutex> locker{resolver.get_mutex()};
        auto it = requests.find(client->get_fd());
        if (it != requests.end()) {
            it->second->cancel();
            requests.erase(client->get_fd());
        }
    }

    {
        auto it = responses.find(client->get_fd());
        if (it != responses.end())
            responses.erase(it);
    }
    
    if (client->has_server()) {
        int server_fd = client->get_server_fd();
        
        servers.erase(server_fd);
        
        queue.delete_event(server_fd, EVFILT_READ);
        queue.delete_event(server_fd, EVFILT_WRITE);
        queue.invalidate_events(server_fd);
    }
    
    queue.invalidate_events(client->get_fd());
    clients.erase(event.ident);
    queue.delete_event(event.ident, EVFILT_WRITE);
    queue.delete_event(event.ident, EVFILT_READ);
    
    delete client;
}

void proxy_server::disconnect_server(struct kevent& event) {
    std::cout << "Disconnect server: " << event.ident << "\n";
    struct server* server = servers[event.ident];
    
    {
        auto it = responses.find(server->get_client_fd());
        if (it != responses.end()) {
            if (it->second.is_cachable() && !it->second.checking()) {
                std::cout << "Added to cache 1!\n";
                cache.insert(it->second.get_request(), it->second);
                std::cout << it->second.get_request() << it->second.get_text();
            }
            responses.erase(it);
        }
    }
    
    queue.invalidate_events(event.ident);
    servers.erase(event.ident);
    queue.delete_event(server->get_fd(), EVFILT_READ);
    queue.delete_event(server->get_fd(), EVFILT_WRITE);
    
    delete server;
}

void proxy_server::connect_client(struct kevent& event) {
    /*
    / If new throw an exception here proxy server must stop
    / Exception will be caught in the function run()
    */
    client* new_client = new client(main_socket);
    clients[new_client->get_fd()] = new_client;
    queue.add_event(new_client->get_fd(), EVFILT_READ, EV_ADD, [this](struct kevent& ev) {
        this->read_from_client(ev);
    });
}

void proxy_server::read_from_client(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_client(event);
        return;
    }
    
    std::cout << "Reading from client: " << event.ident << "\n";
    struct client* client = clients.at(event.ident);
    client->read(event.data);
    
    if (http_request::check_request_end(client->get_buffer())) {
//        auto it = responses.find(client->get_fd());
//        if (it != responses.end() && it->second.is_cachable() && !it->second.checking()) {
//            std::cout << "Added to cache 2!\n";
//            cache.insert(it->second.get_request(), it->second);
//            std::cout << it->second.get_request();
//            it->second.clear_text();
//        }

        responses[client->get_fd()] = http_response(http_request::is_already_cached(client->get_buffer()));
        
        http_request* request = new http_request(client->get_buffer());
        responses[client->get_fd()].set_request(request->get_header());
        
        if (cache.exists(request->get_header())) {
            std::cout << "Trying to use cache!\n";
            request->add_etag(cache[request->get_header()].get_etag());
            responses[client->get_fd()].checking() = true;
        }
        
        if (client->has_server()) {
            if (client->get_host() == request->get_host()) {
                client->send_msg();
                queue.add_event(client->get_server_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE, [this](struct kevent& kev) {
                    this->write_to_server(kev);
                });
                delete request;
                return;
            } else {
                client->disconnect_server();
            }
        }
        request->set_client(client->get_fd());
        requests[client->get_fd()] = request;
        
        std::cout << client->get_buffer();

        resolver.push(request);
        resolver.notify();
    }
}

void proxy_server::read_header_from_server(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }
    
    std::cout << "Reading header from server: " << event.ident << "\n";
    struct server* server = servers.at(event.ident);
    http_response* response = &responses.at(server->get_client_fd());
    std::string msg = std::move(server->read(event.data));
    response->force_append_text(msg);
    if (response->check_header_end()) {
        std::cout << response->get_text() << "\n";
        response->parse_header();
        if (response->checking()) {
            if (response->is_result_304()) {
                std::cout << "Response cached!\n";
                struct client* client = clients.at(server->get_client_fd());
                client->get_buffer() = cache[response->get_request()].get_text();

                queue.add_event(client->get_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE, [this](struct kevent& kev){
                    this->write_to_client(kev);
                });
                return;
            } else {
                std::cout << "Cache has been changed!\n";
                response->checking() = false;
            }
        }

        server->send_msg();
        queue.add_event(server->get_client_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE, [this](struct kevent& kev){
            this->write_to_client(kev);
        });
        
        if (!response->is_cachable())
            response->clear_text();
        
        queue.add_event(event.ident, EVFILT_READ, EV_ADD, [this](struct kevent& kev) {
            this->read_from_server(kev);
        });
    }
}

void proxy_server::read_from_server(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }
    
    std::cout << "Reading from server: " << event.ident << "!\n";
    struct server* server = servers[event.ident];
    std::string msg = server->read(event.data);
    
    if (msg.length() > 0) {
        http_response* response = &responses.at(server->get_client_fd());
        response->append_text(msg);
        
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
        perror("Connecting to server failed!\n");
        return;
    }
    
    struct server* server = servers[event.ident];
    server->write();
    if (server->buffer_empty()) {
        queue.add_event(event.ident, EVFILT_READ, EV_ADD, [this](struct kevent& kev) {
            this->read_header_from_server(kev);
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
    if (read(pipe_fd, tmp.data(), sizeof(char)) == -1) {
        perror("GETTING MESSAGE FROM ANOTHER THREAD ERROR OCCURRED!\n");
    }
    
    http_request* request{std::move(resolver.pop())};
    struct client* client;
    
    std::cout <<"Client: " << request->get_client() << "\n";
    
    if (request->is_canceled()) {
        delete request;
        return;
    }

    client = clients.at(request->get_client());
    requests.erase(client->get_fd());
    
    if (request->is_error()) {
        client->get_buffer() = "HTTP/1.1 400 Bad Request\r\nServer: proxy\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 164\r\nConnection: close\r\n\r\n<html>\r\n<head><title>400 Bad Request</title></head>\r\n<body bgcolor=\"white\">\r\n<center><h1>400 Bad Request</h1></center>\r\n<hr><center>proxy</center>\r\n</body>\r\n</html>";
        queue.add_event(client->get_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE, [this](struct kevent& kev) {
            this->write_to_client(kev);
        });
        delete request;
        return;
    }

    if (!client->has_server()) {
        sockaddr result = std::move(request->get_server());
        int server_socket = -1;
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            perror("Creating socket for server error occurred!\n");
        }
        
        const int set = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) {
            perror("Setting server socket error occurred!\n");
        };
        
        if (fcntl(server_socket, F_SETFL, O_NONBLOCK) == -1) {
            perror("Making server socket non-blocking error occurred!\n");
        }
        
        std::cout << "Connecting to server: " << request->get_host() << "\nIP: "<< inet_ntoa(((sockaddr_in*) &result)->sin_addr) << "\nSocket: " << server_socket << "\n";
        if (connect(server_socket, &result, sizeof(result)) == -1) {
            if (errno != EINPROGRESS) {
                //???: stop proxy-server here
                perror("\nCONNECTING ERROR OCCURRED\n");
                close(server_socket);
                server_socket = -1;
            }
        }

        try {
            struct server* server = new struct server(server_socket);
            server->set_host(request->get_host());
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
    delete request;
    client->send_msg();
}