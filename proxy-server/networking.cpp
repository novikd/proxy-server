//
//  helper.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 15.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#include <arpa/inet.h>


#include "networking.hpp"
#include "http.hpp"

//TODO: rewrite constructor. 
proxy_server::proxy_server(int port) :
    queue(),
    work(true),
    main_socket(init_socket(port))
{
    queue.add_event(main_socket, EVFILT_READ, EV_ADD, [this](struct kevent& kev) {
        this->connect_client(kev);
    });
    queue.add_event(SIGINT, EVFILT_SIGNAL, EV_ADD, [this](struct kevent& event) {
        this->hard_stop();
    });
}

void proxy_server::run() {
    std::cout << "Server started\n";
    
    while (work) {
        int amount = queue.occured();
        
        if (amount == -1) {
            perror("Getting new events error occurred!\n");
        }
        queue.execute();
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
    for (auto client : clients) {
        delete client.second;
    }
    //All the servers must be deleted in that moment
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

//TODO: UNBIND
void proxy_server::disconnect_server(struct kevent& event) {
    std::cout << "Disconnect server: " << event.ident << "\n";
    struct server* server = servers[event.ident];
    queue.invalidate_events(event.ident);
    servers.erase(event.ident);
    queue.delete_event(event.ident, event.filter);
    delete server;
}

void proxy_server::connect_client(struct kevent& event) {
    //If new throw the exception server must stop
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
    struct client* client = clients[event.ident];
    client->read(event.data);
    queue.delete_event(event.ident, EVFILT_WRITE);
    if (client->check_request_end()) {
        http_request request(client->get_buffer());
        std::cout << request.get_header();
//        hosts_to_resolve.push(make_pair(event.ident, request.get_host()));
        if (!client->has_server()) {
            addrinfo hints, *result;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_NUMERICSERV;
            
            std::string host = request.get_host();
            size_t i = host.find(":");
            std::string name(host.substr(0, i)), port = "80";
            
            std::cout << "Host: " << name << "!\n";
            
            if (i != std::string::npos)
                port = host.substr(i + 1);
            
            int error = getaddrinfo(name.c_str(), port.c_str(), &hints, &result);
            if (error == -1) {
                perror("\nRESOLVING ERROR OCCURRED!\n");
            }

            int sock = -1;
            sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if (sock == -1) {
                perror("\nGETTING SOCKET ERROR OCCURRED\n");
            }
            
    //        const int set = 1;
    //        if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) {
    //            perror("\nSETTING SOCKET ERROR OCCURRED!\n");
    //        };
            //TODO: To understand why does not it work.
//            if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
//                perror("\nMAKING SOCKET NON-BLOCKING ERROR OCCURRED\n");
//            }
            
            std::cout << "Connecting to server: " << inet_ntoa(((sockaddr_in*) (result->ai_addr))->sin_addr) << "\n";
            sockaddr addr = *(result->ai_addr);
            if (connect(sock, &addr, sizeof(addr)) == -1) {
                perror("\nCONNECTING ERROR OCCURRED\n");
                close(sock);
                sock = -1;
            }
            
            try {
                struct server* server = new struct server(sock);
                client->bind(server);
                server->bind(client);
                servers[server->get_fd()] = server;
            } catch (...) {
                perror("\nCREATING NEW SERVER ERROR OCCURRED!\n");
            }
        }
        client->send_msg();
        queue.add_event(client->get_server_fd(), EVFILT_WRITE, EV_ADD, [this](struct kevent& kev) {
            this->write_to_server(kev);
        });
    }
}

void proxy_server::read_from_server(struct kevent& event) {
    std::cout << "Reading from server: " << event.ident << "!\n";
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }
    struct server* server = servers[event.ident];
    size_t size = server->read(event.data);
    if (size > 0) {
        server->send_msg();
        queue.add_event(server->get_client_fd(), EVFILT_WRITE, EV_ENABLE, 0, 0, nullptr);
    }
}

void proxy_server::write_to_client(struct kevent& event) {
    std::cout << "Writing to client: " << event.ident << "!\n";
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_client(event);
        return;
    }
    struct client* client = clients[event.ident];
    client->write();
    if (client->has_server())
        client->get_msg();
    queue.add_event(event.ident, event.filter, EV_DISABLE, 0, 0, nullptr);
}

void proxy_server::write_to_server(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }
    struct server* server = servers[event.ident];
// TODO: may be server should have capacity?
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
/*
void resolve_hosts(proxy_server& proxy) {
    while (true) {
        std::unique_lock<std::mutex> locker(proxy.blocker);
        proxy.cv.wait(locker, [&]() {
            return !proxy.clients_for_resolver.empty();
        });
        
        addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        
        client_wrap* client = proxy.clients_for_resolver.front();
        proxy.clients_for_resolver.pop();
        
        if (proxy.hosts.exists(client->host)) {
            result = new addrinfo(proxy.hosts[client->host]);
        } else {
            size_t i = client->host.find(":");
            std::string name(client->host.substr(0, i)), port;
            
            if (i != std::string::npos)
                port = client->host.substr(i);
            
            int error = getaddrinfo(name.c_str(), port.c_str(), &hints, &result);
            
            if (error) {
                perror("\nRESOLVING ERROR OCCURRED\n");
            } else {
                proxy.hosts.insert(client->host, *result);
            }
        }
        
        int sock = -1;
        sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (sock == -1) {
            perror("\nGETTING SOCKET ERROR OCCURRED\n");
        }
        
        if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
            perror("\nMAKING SOCKET NON-BLOCKING ERROR OCCURRED\n");
        }
        
        if (connect(sock, result->ai_addr, result->ai_addrlen) == -1) {
            perror("\nCONNECTING ERROR OCCURRED\n");
            close(sock);
            sock = -1;
        }
        
        try {
            server_handler* tmp = new server_handler(&proxy.queue, sock);
            client->bind(tmp->get_server()); // TODO: race condition
            tmp->get_server()->bind(client);
            proxy.queue.add_event(sock, EVFILT_WRITE, EV_ADD, 0, 0, tmp);
        } catch (...) {
            perror("\nCREATING SERVER ERROR OCCURRED\n");
        }
        
        freeaddrinfo(result);
    }
}
*/