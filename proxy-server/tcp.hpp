//
//  tcp.hpp
//  proxy-server
//
//  Created by Дмитрий Новик on 28.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef tcp_hpp
#define tcp_hpp

#include <assert.h>

struct ipv4_endpoint {
    
    ipv4_endpoint(ipv4_endpoint const& other) = delete;
    ipv4_endpoint& operator=(ipv4_endpoint const& other) = delete;
    
    ipv4_endpoint(int fd) :
        fd(fd)
    {}
    
    ipv4_endpoint(in_addr address, int port = 80) :
        fd(socket(AF_INET, SOCK_STREAM, 0))
    {
        const int set = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
        
        sockaddr_in tmp;
        tmp.sin_family = AF_INET;
        tmp.sin_addr.s_addr = inet_addr(inet_ntoa(address));
        tmp.sin_port = htons(80);
        
        if (connect(fd, (sockaddr*) &tmp, sizeof(tmp)) == -1) {
            perror("Connecting to server error");
        }
    }
    
    int get_fd() const {
        return fd;
    }
    
    ~ipv4_endpoint() {
        close(fd); // read about errno. may be we
    }
    
private:
    int fd;
};

struct server_t {
    server_t(int port) {
        main_socket = socket(PF_INET, SOCK_STREAM, 0);
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        int binded = bind(main_socket, (sockaddr*) &addr, sizeof(addr));
        
        if (binded == -1) {
            perror("Binding arror occured!\n");
        }
        
        listen(main_socket, SOMAXCONN);
    }
    
    ~server_t() {
        close(main_socket);
    }
    
    int get_socket() const {
        return main_socket;
    }
    
    ipv4_endpoint* get_new_client() {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int fd = accept(main_socket, (sockaddr*) &addr, &len);
        return new ipv4_endpoint(fd);
    }
private:
    int main_socket;
};

struct tcp_connection {
    
    tcp_connection(int fd) {
        client = new tcp_client(this, fd);
    }
    
    void init_server(in_addr address, int port = 80) {
        server = new tcp_server(this, address, port);
    }
    
    int get_client_fd() const {
        assert(client != nullptr);
        return client->get_fd();
    }
    
    int get_server_fd() const {
        assert(server != nullptr);
        return server->get_fd();
    }
    
    ~tcp_connection() {
        if (client)
            delete client;
        if (server)
            delete server;
    }
private:
    
//    enum {
//        GETTING_HTTP_REQUEST, MAKING_REQUEST, ANSWERING
//    } state;
    
    struct tcp_client {
        ipv4_endpoint client;
        tcp_connection* connection;
        
        tcp_client(tcp_connection* tcp_connection, int fd) :
        client(fd),
        connection(tcp_connection)
        {}
        
        int get_fd() const {
            return client.get_fd();
        }
        
        ~tcp_client()
        {}
    private:
        std::string host, message;
        
    } *client;
    
    struct tcp_server {
        ipv4_endpoint server;
        tcp_connection* connection;
        
        tcp_server(tcp_connection* tcp_connection , in_addr address, int port = 80) :
            server(address, port),
            connection(tcp_connection)
        {}
        
        int get_fd() const {
            return server.get_fd();
        }
        
        std::string get_msg(size_t len) {
            len = std::min(len, message.length());
            required -= len;
            return message.substr(len);
        }
        
        bool stop() {
            return required == 0;
        }
        
        ~tcp_server()
        {}
        
    private:
        size_t required;
        std::string message;
        
    } *server;
};


#endif /* tcp_h */
