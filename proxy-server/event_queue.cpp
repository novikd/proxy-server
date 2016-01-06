//
//  event_queue.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 28.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//
#include <iostream>

#include "event_queue.hpp"

/********** EVENTS_QUEUE **********/

events_queue::events_queue() :
    kq(kqueue())
{}

int events_queue::add_event(const struct kevent& kev) {
    return kevent(kq, &kev, 1, nullptr, 0, nullptr);
}

int events_queue::add_event(uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data, void* udata) {
    struct kevent kev;
    EV_SET(&kev, ident, filter, flags, fflags, data, udata);
    return kevent(kq, &kev, 1, nullptr, 0, nullptr);
}

int events_queue::add_event(uintptr_t ident, int16_t filter, uint16_t flags, std::function<void(struct kevent&)> handler) {
    return add_event(ident, filter, flags, 0, handler);
}

int events_queue::add_event(uintptr_t ident, int16_t filter, uint16_t flags, intptr_t data, std::function<void(struct kevent&)> handler) {
    return add_event(ident, filter, flags, 0, data, handler);
}

int events_queue::add_event(uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data, std::function<void(struct kevent&)> handler) {
    handlers[id{ident, filter}] = handler;
    return add_event(ident, filter, flags, fflags, data, nullptr);
}


int events_queue::delete_event(uintptr_t ident, int16_t filter) {
    auto it = handlers.find(id{ident, filter});
    if (it != handlers.end()) {
        handlers.erase(it);
        return add_event(ident, filter, EV_DELETE, 0, 0, nullptr);
    }
    return 0;
}

int events_queue::occured() {
    return kevent(kq, nullptr, 0, event_list, EVLIST_SIZE, nullptr);
}

void events_queue::execute() {
    int amount = kevent(kq, nullptr, 0, event_list, EVLIST_SIZE, nullptr);
    
    if (amount == -1) {
        perror("Getting number of events error!\n");
    }
    
    invalid.clear();
    
    std::cout << "\nNEW EXECUTION!\n";
    for (int i = 0; i < amount; ++i) {
        bool is_valid = true;
        
        for (auto e : invalid)
            if (e == event_list[i].ident) {
                is_valid = false;
                break;
            }
        
        if (is_valid) {
            std::function<void(struct kevent&)> handler = handlers[id{event_list[i]}];
            handler(event_list[i]);
        }
    }
}

void events_queue::invalidate_events(uintptr_t id) {
    invalid.push_back(id);
    std::cout << "APPENED: " << invalid.size() << "\n";
}

events_queue::~events_queue() {
    close(kq);
}

/********** ID **********/

id::id() :
    ident(0),
    filter(0)
{}

id::id(uintptr_t new_ident, int16_t new_filter) :
    ident(new_ident),
    filter(new_filter)
{}

id::id(struct kevent const& event) :
    ident(event.ident),
    filter(event.filter)
{}

bool operator==(id const& frs, id const& snd) {
    return frs.ident == snd.ident && frs.filter == snd.filter;
}

bool operator<(id const& frs, id const& snd) {
    return frs.ident < snd.ident || (frs.ident == snd.ident && frs.filter < snd.filter);
}
