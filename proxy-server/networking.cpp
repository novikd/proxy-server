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
#include "exceptions.hpp"

const std::string BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\nServer: proxy\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 164\r\nConnection: close\r\n\r\n<html>\r\n<head><title>400 Bad Request</title></head>\r\n<body bgcolor=\"white\">\r\n<center><h1>400 Bad Request</h1></center>\r\n<hr><center>proxy</center>\r\n</body>\r\n</html>";

const std::string NOT_FOUND = "HTTP/1.1 404 Not Found\r\nServer: proxy\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 160\r\nConnection: close\r\n\r\n<html>\r\n<head><title>404 Not Found</title></head>\r\n<body bgcolor=\"white\">\r\n<center><h1>404 Not Found</h1></center>\r\n<hr><center>proxy</center>\r\n</body>\r\n</html>";

namespace sockets {
    void make_nonblocking(int fd) {
        if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
            throw custom_exception("Making socket non-blocking error occurred!\n");
        }
    }
    
    int create_listening_socket(int port) {
        int new_socket = socket(PF_INET, SOCK_STREAM, 0);
        
        if (new_socket == -1) {
            throw custom_exception("Creating socket error occurred!");
        }
        
        const int set = 1;
        if (setsockopt(new_socket, SOL_SOCKET, SO_REUSEPORT, &set, sizeof(set)) == -1) {
            perror("\nSetting main socket SO_REUSEPORT error occurred!\n");
        };

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(new_socket, (sockaddr*) &addr, sizeof(addr)) == -1) {
            throw custom_exception("Binding error occured!\n");
        }
        
        make_nonblocking(new_socket);
        
        if (listen(new_socket, SOMAXCONN) == -1) {
            throw custom_exception("Starting listening error occurred!\n");
        }
        
        return new_socket;
    }
    
    int create_connect_socket(sockaddr addr) {
        int server_socket = socket(AF_INET, SOCK_STREAM, 0);

        if (server_socket == -1) {
            perror("Creating socket for server error occurred!\n");
            return server_socket;
        }
        
        const int set = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) {
            perror("Setting server socket error occurred!\n");
            close(server_socket);
            return -1;
        };

        try {
            make_nonblocking(server_socket);
        } catch (...) {
            close(server_socket);
            return -1;
        }
        
        std::cout << "Connecting to IP: " << inet_ntoa(((sockaddr_in*) &addr)->sin_addr) << "\nSocket: " << server_socket << "\n";
        if (connect(server_socket, &addr, sizeof(addr)) == -1) {
            if (errno != EINPROGRESS) {
                close(server_socket);
                perror("\nConnecting error occurred!");
                return -1;
            }
        }
        
        return server_socket;
    }
}

proxy_server::proxy_server(int port) :
main_socket(sockets::create_listening_socket(port)),
    queue(),
    work(true),
    stopped(false),
    resolver()
{
    /*
     / Pipe will help to get messages from resolver's thread
     / I had to use pipe, because EVFILT_USER never triggers
     / @param pipe_fd -- this socket is used for listening to(?) messages
     */

    int fds[2];
    if (pipe(fds) == -1) {
        throw custom_exception("Creating a pipe error occurred!\n");
    }

    sockets::make_nonblocking(fds[0]);
    sockets::make_nonblocking(fds[1]);
    
    resolver.set_fd(fds[1]);
    pipe_fd.set_fd(fds[0]);
    
    /* Event for connecting new clients */
    queue.add_event([this](struct kevent& kev) {
        this->connect_client(kev);
    }, main_socket.get_fd(), EVFILT_READ, EV_ADD);
    
    /* Event for getting messages from resolver's threads */
    queue.add_event([this](struct kevent& kev) {
        this->on_host_resolved(kev);
    }, pipe_fd.get_fd(), EVFILT_READ, EV_ADD);
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
        // TODO: what is the point of this hard_stop?
        hard_stop();
    }
}

void proxy_server::start() {
    work = true;
}

void proxy_server::hard_stop() {
    // TODO: setting this to false does not guarantee that
    // all threads of host_resolver will finish requests processing
    // by the time we start deleting requests

    // we should have some function (e.g. host_resolver::stop) that
    // returns only when all threads of resolver are terminated
    work = false;
    resolver.stop();
}

void proxy_server::stop() {
    queue.invalidate_events(main_socket.get_fd());
    queue.delete_event(main_socket.get_fd(), EVFILT_READ);
    stopped = true;
}

void proxy_server::handle_signal(int sig, std::function<void(struct kevent&)> handler) {
    queue.add_event(handler, sig, EVFILT_SIGNAL, EV_ADD);
    signal(sig, SIG_IGN);
}

proxy_server::~proxy_server() {
    resolver.stop();
    std::cout << "Server stopped!\n";
}

void proxy_server::disconnect_client(struct kevent& event) {
    std::cout << "Disconnect client: " << event.ident << "\n";
    struct client* client = clients.at(event.ident).get(); //
    
    {
        auto it = requests.find(client->get_fd());
        if (it != requests.end()) {
            resolver.cancel(it->second);
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
        std::cout << "Disconnect server(with client): " << server_fd << "\n";
        
        servers.erase(server_fd);
        
        queue.delete_event(server_fd, EVFILT_READ);
        queue.delete_event(server_fd, EVFILT_WRITE);
        queue.invalidate_events(server_fd);
    }
    
    queue.invalidate_events(client->get_fd());

    queue.delete_event(event.ident, EVFILT_WRITE);
    queue.delete_event(event.ident, EVFILT_READ);
    queue.delete_event(event.ident, EVFILT_TIMER);
    
    clients.erase(event.ident);
    if (stopped && clients.size() == 0)
        hard_stop();
}

void proxy_server::disconnect_server(struct kevent& event) {
    std::cout << "Disconnect server: " << event.ident << "\n";
    struct server* server = servers.at(event.ident);
    update_timer(server->get_client_fd());
    
    {
        auto it = responses.find(server->get_client_fd());
        if (it != responses.end()) {
            if (it->second.is_cachable() && !it->second.checking()) {
                std::cout << "Added to cache!\n";
                cache.insert(it->second.get_request(), it->second);
            }
            responses.erase(it);
        }
    }
    
    queue.invalidate_events(event.ident);
    servers.erase(event.ident);
    queue.delete_event(server->get_fd(), EVFILT_READ);
    queue.delete_event(server->get_fd(), EVFILT_WRITE);
    
    server->disconnect();
}

void proxy_server::connect_client(struct kevent& event) {
    /*
    / If new throw an exception here proxy server must stop.
    / Exception will be caught in the function run()
    */
    client* new_client = new client(main_socket.get_fd());
    clients[new_client->get_fd()] = std::move(std::unique_ptr<client>(new_client));
    queue.add_event([this](struct kevent& ev) {
        this->read_from_client(ev);
    }, new_client->get_fd(), EVFILT_READ, EV_ADD);
    queue.add_event([this](struct kevent& kev) {
        this->disconnect_client(kev);
    }, new_client->get_fd(), EVFILT_TIMER, EV_ADD, NOTE_SECONDS, 600);
}

void proxy_server::read_from_client(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_client(event);
        return;
    }
    std::cout << "Reading from client: " << event.ident << "\n";

    struct client* client = clients.at(event.ident).get();
    update_timer(client->get_fd());

    client->read(event.data);
    
    if (http_request::check_request_end(client->get_buffer())) {
        responses[client->get_fd()] = http_response(http_request::is_already_cached(client->get_buffer()));

        std::unique_ptr<http_request> request(new (std::nothrow) http_request(client->get_buffer()));
        if (!request) {
            client->get_buffer() = BAD_REQUEST;
            queue.add_event([this](struct kevent& kev) {
                this->write_to_client(kev);
            }, client->get_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE);
            return;
        }

        responses[client->get_fd()].set_request(request->get_url());
        
        if (cache.exists(request->get_url())) {
            std::cout << "Trying to use cache!\n";
            request->add_etag(cache[request->get_header()].get_etag());
            responses[client->get_fd()].checking() = true;
        }
        
        if (client->has_server()) {
            if (client->get_host() == request->get_host()) {
                client->send_msg();
                queue.add_event([this](struct kevent& kev) {
                    this->write_to_server(kev);
                }, client->get_server_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE);
                return;
            } else {
                client->unbind();
            }
        }
        request->set_client(client->get_fd());
        requests[client->get_fd()] = request.get();

        resolver.push(std::move(request));
    }
}

void proxy_server::read_header_from_server(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }
    std::cout << "Reading header from server: " << event.ident << "\n";
    struct server* server = servers.at(event.ident);
    update_timer(server->get_client_fd());
    http_response* response = &responses.at(server->get_client_fd());
    
    std::string msg = std::move(server->read(event.data));
    response->force_append_text(msg);
    
    if (response->check_header_end()) {
        response->parse_header();
        
        if (response->checking()) {
            if (response->is_result_304()) {
                std::cout << "Response cached!\n";
                struct client* client = clients.at(server->get_client_fd()).get();
                client->get_buffer() = cache[response->get_request()].get_text();

                queue.add_event([this](struct kevent& kev){
                    this->write_to_client(kev);
                }, client->get_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE);
                return;
            } else {
                std::cout << "Cache has been changed!\n";
                response->checking() = false;
            }
        }

        if (!response->is_cachable())
            response->clear_text();
        
        server->send_msg();

        queue.add_event([this](struct kevent& kev){
            this->write_to_client(kev);
        }, server->get_client_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE);
        queue.add_event([this](struct kevent& kev) {
            this->read_from_server(kev);
        }, event.ident, EVFILT_READ, EV_ADD);
    }
}

void proxy_server::read_from_server(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }

    std::cout << "Reading from server: " << event.ident << "\n";
    struct server* server = servers.at(event.ident);
    update_timer(server->get_client_fd());

    std::string msg = std::move(server->read(event.data));
    
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

    std::cout << "Writing to client: " << event.ident << "\n";
    struct client* client = clients.at(event.ident).get();
    update_timer(client->get_fd());

    client->write();
    if (client->has_server())
        client->get_msg();
    if (client->get_buffer_size() == 0)
        queue.add_event(event.ident, event.filter, EV_DISABLE, 0, 0, nullptr);
}

void proxy_server::write_to_server(struct kevent& event) {
    if (event.flags & EV_EOF && event.data == 0) {
        disconnect_server(event);
        return;
    }

    std::cout << "Writing to server: " << event.ident << "\n";
    struct server* server = servers.at(event.ident);
    update_timer(server->get_client_fd());
    
    int err;
    socklen_t len = sizeof(err);
    if (getsockopt(static_cast<int>(event.ident), SOL_SOCKET, SO_ERROR, &err, &len) == -1 || err != 0) {
        //???: To stop proxy-server at that moment?
        disconnect_server(event);
        perror("Connecting to server failed!\n");
        return;
    }

    server->write();
    if (server->buffer_empty()) {
        queue.add_event([this](struct kevent& kev) {
            this->read_header_from_server(kev);
        }, event.ident, EVFILT_READ, EV_ADD);
        queue.delete_event(event.ident, event.filter);
        queue.add_event([this](struct kevent& kev) {
            this->write_to_client(kev);
        }, server->get_client_fd(), EVFILT_WRITE, EV_ADD);
    }
}

void proxy_server::on_host_resolved(struct kevent& event) {
    std::cout << "Host resoved (callback)\n";
    char tmp;
    if (read(pipe_fd.get_fd(), &tmp, sizeof(tmp)) == -1) {
        perror("Getting message from another thread error occurred!\n");
    }
    
    std::unique_ptr<http_request> request = resolver.pop();
    
    std::cout <<"Client: " << request->get_client() << "\n";
    
    if (request->is_canceled()) {
        return;
    }

    struct client* client = clients.at(request->get_client()).get();
    update_timer(client->get_fd());
    requests.erase(client->get_fd());
    
    if (request->is_error()) {
        client->get_buffer() = BAD_REQUEST;
        queue.add_event([this](struct kevent& kev) {
            this->write_to_client(kev);
        }, client->get_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE);
        return;
    }


    sockaddr result = std::move(request->get_server());
    int server_socket = sockets::create_connect_socket(result);
    if (server_socket == -1) {
        client->get_buffer() = BAD_REQUEST;
        queue.add_event(client->get_fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        return;
    }

    struct server* server = new (std::nothrow) struct server(server_socket);
    if (!server) {
        perror("\nCreating new server error occurred!\n");
        return;
    }
    
    server->set_host(request->get_host());
    client->bind(server);
    server->bind(client);
    servers[server->get_fd()] = server;
    queue.add_event([this](struct kevent& kev) {
        this->write_to_server(kev);
    }, server_socket, EVFILT_WRITE, EV_ADD);

    client->send_msg();
}

void proxy_server::update_timer(int client_fd) {
    queue.add_event(client_fd, EVFILT_TIMER, EV_DELETE, NOTE_SECONDS, 600, nullptr);
    queue.add_event(client_fd, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, 600, nullptr);
}
