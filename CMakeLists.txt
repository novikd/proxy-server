cmake_minimum_required(VERSION 2.8)
project(proxy-server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

set(SOURCE_FILES
"proxy-server/networking.cpp"
"proxy-server/networking.hpp"
"proxy-server/http.cpp"
"proxy-server/http.hpp"
"proxy-server/sockets.cpp"
"proxy-server/sockets.hpp"
"proxy-server/event_queue.cpp"
"proxy-server/event_queue.hpp"
"proxy-server/server.cpp"
"proxy-server/lru_cache.hpp"
"proxy-server/exceptions.hpp"
"proxy-server/host_resolver.hpp"
"proxy-server/host_resolver.cpp")

add_executable(proxy ${SOURCE_FILES})
