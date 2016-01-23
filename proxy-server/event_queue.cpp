//
//  event_queue.cpp
//  proxy-server
//
//  Created by Дмитрий Новик on 28.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//
#include <iostream>

#include "exceptions.hpp"
#include "event_queue.hpp"

/********** EVENTS_QUEUE **********/

events_queue::events_queue() :
    kq(kqueue())
{}

void events_queue::add_event(const struct kevent& kev) {
    if (kevent(kq.get_fd(), &kev, 1, nullptr, 0, nullptr) == -1)
        throw custom_exception("Kqueue error occurred!");
}

void events_queue::add_event(uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data, void* udata) {
    struct kevent kev;
    EV_SET(&kev, ident, filter, flags, fflags, data, udata);

    if (kevent(kq.get_fd(), &kev, 1, nullptr, 0, nullptr) == -1)
        throw custom_exception("Kqueue error occurred!");
}

void events_queue::add_event(std::function<void(struct kevent&)> handler, uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data) {
    handlers[id{ident, filter}] = handler;
    
    add_event(ident, filter, flags, fflags, data, nullptr);
}


void events_queue::delete_event(uintptr_t ident, int16_t filter) {
    auto it = handlers.find(id{ident, filter});
    if (it != handlers.end()) {
        handlers.erase(it);
        add_event(ident, filter, EV_DELETE, 0, 0, nullptr);
    }
}

int events_queue::occured() {
    return kevent(kq.get_fd(), nullptr, 0, event_list, EVLIST_SIZE, nullptr);
}

void events_queue::execute() {
    int amount = kevent(kq.get_fd(), nullptr, 0, event_list, EVLIST_SIZE, nullptr);
    
    if (amount == -1) {
        perror("Getting number of events error!\n");
    }
    
    invalid.clear();

    for (int i = 0; i < amount; ++i) {
        bool is_valid = invalid.find(event_list[i].ident) == invalid.end();
        
        if (is_valid) {
            std::function<void(struct kevent&)> handler = handlers.at(id{event_list[i]});
            handler(event_list[i]);
        }
    }
}

void events_queue::invalidate_events(uintptr_t id) {
    invalid.insert(id);
}

events_queue::~events_queue() {}

/********** ID **********/

events_queue::id::id() :
    ident(0),
    filter(0)
{}

events_queue::id::id(uintptr_t new_ident, int16_t new_filter) :
    ident(new_ident),
    filter(new_filter)
{}

events_queue::id::id(struct kevent const& event) :
    ident(event.ident),
    filter(event.filter)
{}

bool operator==(events_queue::id const& frs, events_queue::id const& snd) {
    return frs.ident == snd.ident && frs.filter == snd.filter;
}

bool operator<(events_queue::id const& frs, events_queue::id const& snd) {
    return frs.ident < snd.ident || (frs.ident == snd.ident && frs.filter < snd.filter);
}
