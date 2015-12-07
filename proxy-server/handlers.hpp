//
//  handlers.hpp
//  proxy-server
//
//  Created by Дмитрий Новик on 28.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef handlers_h
#define handlers_h

#include "event_queue.hpp"
#include "tcp.hpp"

struct client_handler : handler
{
    client_handler(events_queue* main_queue, tcp_wrap* new_tcp) :
        queue(main_queue),
        tcp(new_tcp)
    {}
    
    void handle(struct kevent& event) override {
        if (event.flags & EV_EOF && event.data == 0) {
            if (event.ident == tcp->connection->get_client_fd()) {
                std::cout << "\nCLIENT DISCONNECTED: " << tcp->connection->get_client_fd() << "\n";
                queue->invalidate_events(event.ident);
                delete this;
            } else {
                std::cout << "\nSERVER DISCONNECTED: " << tcp->connection->get_server_fd() << "\n";
                if (tcp->state == tcp_wrap::GETTING_ANSWER) {
                    tcp->next_state();
                }
                queue->invalidate_events(event.ident);
                tcp->close_server();
            }
            return;
        }
//        int fd = static_cast<int>(event.ident);
        switch(tcp->state) {
            case tcp_wrap::GETTING_REQUEST:
                std::cout << "\nGETTING_REQUEST " << event.ident << "\n";
                if (event.filter & EVFILT_READ) {
                    char* buffer = new char[event.data];
                    recv(tcp->connection->get_client_fd(), buffer, event.data, 0);
                    tcp->append_request(buffer);
                    if (check_request_end()) {
                        std::cout << tcp->get_request();
                        parse_host();
                        tcp->next_state();
                        in_addr temp = resolve();
                        tcp->init_server(temp);
//                        queue->add_event(fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                        queue->add_event(tcp->connection->get_server_fd(), EVFILT_WRITE, EV_ADD, 0, 0, this);
                    }
                    delete[] buffer;
                }
                break;
            case tcp_wrap::GETTING_RESPONSE:
                if (event.filter & EVFILT_WRITE) {
                    std::cout << "\nGETTING_RESPONSE\n";
                    send(tcp->connection->get_server_fd(), tcp->get_request().c_str(), tcp->get_request().size(), 0);
                    tcp->set_request("");
                    tcp->next_state();
                    queue->add_event(tcp->connection->get_server_fd(), EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
                    queue->add_event(tcp->connection->get_server_fd(), EVFILT_READ, EV_ADD, 0, 0, this);
                }
                break;
            case tcp_wrap::GETTING_ANSWER:
                std::cout << "\nENTERED\n";
                if (event.filter == EVFILT_READ) {
                    std::cout << "\nGETTING_ANSWER " << event.ident << "\n";
                    char* buffer = new char[event.data];
                    bzero(buffer, event.data);
                    
                    if (event.ident != tcp->connection->get_server_fd()) {
                        perror("INVALID FileDescriptor ERROR");
                    }
                    
//                    while (recv(tcp->connection->get_server_fd(), buffer, event.data, 0) > 0) {
//                        tcp->append_answer(buffer);
//                        bzero(buffer, event.data);
//                    }
                    size_t size = recv(tcp->connection->get_server_fd(), buffer, event.data, 0);
                    if (size == -1) {
                        perror("\nREADING MESSAGE ERROR!\n");
                    }
//                    send(tcp->connection->get_client_fd(), buffer, size, 0);
                    tcp->append_answer(std::string(buffer, size)); // size deleted, I hope it will work
                    std::cout << buffer;
                    delete[] buffer;
                    
                    if (check_answer_end()) {
                        std::cout << tcp->get_answer();
                        tcp->next_state();
                        queue->add_event(tcp->connection->get_server_fd(), EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                        queue->add_event(tcp->connection->get_client_fd(), EVFILT_WRITE, EV_ADD, 0, 0, this);
//                        tcp->close_server();
//                        delete [] buffer;
                    }
                }
                break;
            case tcp_wrap::ANSWERING:
                if (event.filter & EVFILT_WRITE) {
                    std::cout << "\nANSWERING " << event.ident << "\n";
                    send(tcp->connection->get_client_fd(), tcp->get_answer().c_str(), tcp->get_answer().size(), 0);
//                    std::cout << tcp->get_answer();
                    tcp->set_request("");
                    tcp->set_answer("");
                    tcp->next_state();
                    queue->add_event(event.ident, event.filter, EV_DELETE, 0, 0, nullptr);
                    queue->add_event(tcp->connection->get_client_fd(), EVFILT_READ, EV_ADD, 0, 0, this);
                }
                break;
            default:
                tcp->next_state();
//                std::cout << "\nFINISH\n";
//                queue->add_event(fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
//                
////                delete this;
//                break;
        }
    }
    
    ~client_handler() {
        delete tcp;
    }
private:
    events_queue* queue;
    tcp_wrap* tcp;
    
    void parse_host() {
        std::string request(tcp->get_request());
        size_t i = request.find("Host:");
        if (i == std::string::npos) {
            std::cout << "\nBAD REQUEST FOUND\n";
            return;
        }
        i += 6;
        size_t j = request.find("\r\n", i);
        std::string host = request.substr(i, j - i);
        tcp->set_host(request.substr(i, j - i));
        std::cout << "Host for request: " << tcp->get_host() << "\n";
    }
    
    bool check_request_end() {
        size_t i;
        std::string request(tcp->get_request());
        if ((i = request.find("\r\n\r\n")) == std::string::npos) {
            return false;
        }
        
        if (request.find("POST") == std::string::npos)
            return true;
        
        i += 4;
        
        if (request.find("\r\n\r\n", i) != std::string::npos)
            return true;
        
        return false;
    }
    
    bool check_answer_end() {
        std::string answer = tcp->get_answer();
        if (answer == "") return false;
        
        if (answer.find("Transfer-Encoding: chunked") != std::string::npos) {
            return answer.find("\r\n0\r\n\r\n") != std::string::npos;
        } else if (answer.find("Content-Length:") != std::string::npos) {
            size_t i = answer.find("Content-Length:");
            i += 16;
            size_t content_length = 0;
            
            while (answer[i] != '\r') {
                content_length *= 10;
                content_length += answer[i++] - '0';
            }
            i = answer.find("\r\n\r\n");
            i += 4;
            std::cout << "\nNEED: " << content_length << " ITERATORS: " << answer.substr(i).length() << " " << "\n";
            return answer.substr(i).length() == content_length; // -1 ?
        } else {
            return answer.find("\r\n\r\n") != std::string::npos;
        }
    }
    
    in_addr resolve() {
        struct hostent* hostent = gethostbyname(tcp->get_host().c_str());
        if (hostent == nullptr) {
            perror("Resolving host error occured!\n");
        }
        
        in_addr addr = *(in_addr*)(hostent->h_addr);
        //        if (inet_aton(hostent->h_addr, &addr) == 0) {
        //            std::cout << hostent->h_addr << "\n";
        //            perror("Invalid IP address!\n");
        //        }
        std::cout << "Trying to connect: " << inet_ntoa(addr) << "\n";
        return addr;
    }

};

struct connector : handler
{
    connector(events_queue* new_queue) :
        queue(new_queue)
    {}
    
    void handle(struct kevent& event) override {
        sockaddr_in addr;
        socklen_t size = sizeof(addr);
        int fd = accept(static_cast<int>(event.ident), (sockaddr*) &addr, &size);
        if (fd == -1) {
            perror("Connection error: Can't connect to new user!\n");
        }
        std::cout << "CLIENT CONNECTED: " << fd << "\n";
        queue->add_event(fd, EVFILT_READ, EV_ADD, 0, 0, new client_handler(queue, new tcp_wrap(fd)));
    }
    
private:
    events_queue* queue;
};

#endif /* handlers_h */
