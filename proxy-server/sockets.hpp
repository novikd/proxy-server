//
//  sockets.hpp
//  proxy-server
//
//  Created by Дмитрий Новик on 23.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef sockets_hpp
#define sockets_hpp

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
    
    ~client_socket() {
        close(client_fd);
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
        
#warning TODO: Make this socket non-blocking
        if (connect(server_fd, (sockaddr*) &tmp, sizeof(tmp)) == -1) {
            throw new socket_exception("Connecting to server error occurred!");
        }
    }
    
    server_socket(int fd) :
        server_fd(fd)
    {}
    
#warning ???: may be it should throw exception
    size_t write(std::string msg) {
        size_t length = send(server_fd, msg.data(), msg.size(), 0);
        
        if (length == -1) {
            throw new socket_exception("Sending to the server error occurred!");
        }
        
        return length;
    }
    
    std::string read(size_t buffer_size) {
        std::vector<char> buffer(buffer_size);
        size_t length = recv(server_fd, buffer.data(), buffer_size, 0);
        
        return std::string(buffer.cbegin(), buffer.cbegin() + length);
    }
    
    ~server_socket() {
        close(server_fd);
    }
    
    int server_fd;
};

struct server_wrap;

struct socket_holder {
    virtual bool has_free_capacity() = 0;
    virtual void append_buffer(std::string msg) = 0;
    virtual bool buffer_empty() = 0;
    virtual void read(size_t size) = 0;
    virtual size_t write() = 0;
    virtual void send_msg() = 0;
    virtual int get_fd() = 0;
    virtual void bind(socket_holder*) = 0;
    virtual void unbind() = 0;
    virtual ~socket_holder() = default;
};

struct client_wrap : socket_holder {
    static const size_t buffer_size = 20000;
    
    client_wrap(int fd) :
    client(fd)
    {
        server = nullptr;
    }
    
    bool has_free_capacity() {
        return buffer.size() < buffer_size;
    }
    
    void append_buffer(std::string msg) {
        buffer.append(msg);
    }
    
    bool buffer_empty() {
        return buffer.size() == 0;
    }
    
    void read(size_t size) {
        std::string result = client.read(size);
        buffer.append(result);
    }
    
    size_t write() {
        size_t length = client.write(buffer);
        
        return length;
    }
    
    int get_fd() {
        return client.client_fd;
    }
    
    bool has_server() {
        return server != nullptr;
    }
    
    int get_server_fd() {
        return server->get_fd();
    }
    
    void send_msg() {
        assert(server != nullptr);
        if (server->has_free_capacity()) {
            server->append_buffer(buffer);
            buffer = "";
        }
    }
    
    void bind(socket_holder* server) {
        this->server = server;
    }
    
    void unbind() {
        this->server = nullptr;
    }
    
    ~client_wrap() {
        if (server)
            delete server;
        server = nullptr;
    }
    
    std::string buffer, host;
    client_socket client;

    socket_holder* server;
};

struct server_wrap : socket_holder {
    static const size_t buffer_size = 20000;
    
    server_wrap(in_addr address) :
        server(address)
    {
        client = nullptr;
    }
    
    server_wrap(int fd) :
        server(fd)
    {}
    
    bool has_free_capacity() {
        return buffer.size() < buffer_size;
    }
    
    void append_buffer(std::string msg) {
        buffer.append(msg);
    }
    
    bool buffer_empty() {
        return buffer.size() == 0;
    }
    
    void read(size_t buffer_size) {
        std::string result = server.read(buffer_size);
        buffer.append(result);
    }
    
    size_t write() noexcept {
        try {
            size_t length = server.write(buffer);
            if (length != 0)
                buffer = buffer.substr(length);
            
            return length;
        } catch(...) {
            return -1;
        }
    }
    
    int get_fd() {
        return server.server_fd;
    }
    
    void send_msg() {
        assert(client != nullptr);
        if (client->has_free_capacity()) {
            client->append_buffer(buffer);
            buffer = "";
        }
    }
    
    void bind(socket_holder* client) {
        this->client = client;
    }
    
    void unbind() {
        this->client = nullptr;
    }
    
    ~server_wrap() {
        if (client)
            client->unbind();
        unbind();
    }
    
    std::string buffer;
    server_socket server;

    socket_holder* client;
};

#endif /* sockets_hpp */
