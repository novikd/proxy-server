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
        
        //Program sleeps here, but socket is non-blocking
        //TODO: Solve this problem.
        if (connect(fd, (sockaddr*) &tmp, sizeof(tmp)) == -1) {
            perror("Connecting to server error");
        }
    }
    
    int get_fd() const {
        return fd;
    }
    
    //TODO: Find situation, when destructor trys to close invalid socket.
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
        server = nullptr;
    }
    
    void init_server(in_addr address, int port = 80) {
        server = new tcp_server(this, address, port);
    }
    
    void close_server() {
        if (server) {
            delete server;
            server = nullptr;
        }
    }
    
    int get_client_fd() const {
        assert(client != nullptr);
        return client->get_fd();
    }
    
    int get_server_fd() const {
//        assert(server != nullptr);
        if (server != nullptr)
            return server->get_fd();
        else
            return -1;
    }
    
    bool is_server_inited() {
        return server != nullptr;
    }
    
    ~tcp_connection() {
        if (client)
            delete client;
        if (server)
            delete server;
    }
private:
    
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

        ~tcp_server()
        {}
        
    } *server;
};

struct tcp_wrap {
    
    tcp_wrap(tcp_connection* connection) :
        connection(connection)
    {
        state = GETTING_REQUEST;
    }
    
    tcp_wrap(int fd)
    {
        connection = new tcp_connection(fd);
        state = GETTING_REQUEST;
    }
    
    /* Functions for server connection controling */
    void init_server(in_addr addr) {
        if (connection->is_server_inited())
            connection->close_server();
        connection->init_server(addr);
    }
    
    void close_server() {
        connection->close_server();
    }
    
    /* Functions for setting string */
    void set_host(std::string host) {
        this->host = host;
    }
    
    void set_request(std::string temp) {
        request = temp;
    }
    
    void set_answer(std::string temp) {
        answer = temp;
    }
    
    void append_request(std::string temp) {
        request.append(temp);
    }
    
    void append_answer(std::string temp) {
        answer.append(temp);
    }
    
    std::string get_host() {
        return host;
    }
    
    std::string& get_request() {
        return request;
    }
    
    std::string& get_answer() {
        return answer;
    }
    
    bool is_finish() {
        return state == FINISH;
    }
    
    void next_state() {
        switch(state) {
            case GETTING_REQUEST:
                state = MAKING_REQUEST;
                break;
            case MAKING_REQUEST:
                state = GETTING_RESPONSE;
                break;
            case GETTING_RESPONSE:
                state = ANSWERING; //
                break;
            case GETTING_ANSWER:
                state = ANSWERING; //TODO: DELETE THIS
                break;
            case ANSWERING: 
                state = FINISH;
                break;
            default:
                state = GETTING_REQUEST;
        }
    }

    enum {
        GETTING_REQUEST, MAKING_REQUEST, GETTING_RESPONSE, GETTING_ANSWER, ANSWERING, FINISH
    } state;
    
    ~tcp_wrap() {
        delete connection;
    }
    
    tcp_connection* connection;
private:
    std::string host, request, answer;
};

struct socket_exception : std::exception {
    socket_exception(const std::string& str) :
        error_info(str)
    {}
    
    const char* what() const noexcept override {
        return error_info.c_str();
    }
    
    std::string error_info;
};

struct client_socket {
    client_socket(int fd) {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        
        client_fd = accept(fd, (sockaddr*) &addr, &len);
        if (client_fd == -1) {
            throw new socket_exception("Accepting connection error occurred!");
        }
    }
    
    
    //May be change size_t to ptrdiff_t?
    size_t write(std::string msg) {
        size_t length = send(client_fd, msg.data(), msg.size(), 0);
        
        return length;
    }
    
    std::string read(size_t buffer_size) {
        std::vector<char> buffer(buffer_size);
        size_t length = recv(client_fd, buffer.data(), buffer_size, 0);
        
        return std::string(buffer.cbegin(), buffer.cend() + length);
    }
    
    int client_fd;
};

struct server_socket {
    server_socket(in_addr addr) :
        server_fd(socket(AF_INET, SOCK_STREAM, 0))
    {
        const int set = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
        
        sockaddr_in tmp;
        tmp.sin_family = AF_INET;
        tmp.sin_addr.s_addr = inet_addr(inet_ntoa(addr));
        tmp.sin_port = htons(80);
        
        //TODO: Solve this problem.
        if (connect(server_fd, (sockaddr*) &tmp, sizeof(tmp)) == -1) {
            throw new socket_exception("Connecting to server error occurred!");
        }
    }
    
    
    
    int server_fd;
};

#endif /* tcp_h */
