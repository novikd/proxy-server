//
//  helper.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 15.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

//#include <stdio.h>
//#include <stdint.h>
//#include <sys/time.h>    -
//#include <sys/types.h>   -
//#include <sys/event.h>   -
//#include <sys/socket.h>  -
//#include <netinet/in.h>  -
//#include <string>
//#include <netdb.h>
//#include <arpa/inet.h>   -
//#include <unistd.h>

#include "networking.hpp"

//TODO: rewrite constructor. 
proxy_server::proxy_server(int port) :
    queue(),
    work(true),
    main_socket(init_socket(port))
{
/*    struct connector* tmp = new connector(&queue, main_socket);
    queue.add_event(main_socket, EVFILT_READ, EV_ADD, tmp);
    queue.add_event(SIGINT, EVFILT_SIGNAL, EV_ADD, new signal_handler(&queue, tmp)); // TODO: when signal handlers dies
    queue.add_event(SIGTERM, EVFILT_SIGNAL, EV_ADD, new signal_handler(&queue, tmp));
*/
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
#warning FIXME: memory leak here
    queue.add_event(main_socket, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
}

proxy_server::~proxy_server() {
    for (auto client : clients) {
        delete client.second;
    }
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
    struct client* client = clients[event.ident];
    if (client->has_server())
        queue.invalidate_events(client->get_server_fd());
    queue.invalidate_events(client->get_fd());
    clients.erase(event.ident);
    delete client;
}

void proxy_server::disconnect_server(struct kevent& event) {
    struct server* server = servers[event.ident];
    queue.invalidate_events(event.ident);
    servers.erase(event.ident);
    delete server;
}

void proxy_server::connect_client(struct kevent& event) {
    //If new throw the exception server must stop
    client* new_client = new client(main_socket);
    clients[new_client->get_fd()] = new_client;
//    queue.add_event(new_client->get_fd(), EVFILT_READ, EV_ADD, [this](struct kevent& ev) {
//        this->read_from_client(ev);
//    });
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