//
//  event_queue.h
//  proxy-server
//
//  Created by Дмитрий Новик on 17.11.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef event_queue_hpp
#define event_queue_hpp

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <set>
#include <functional>
#include <map>

struct events_queue {
    events_queue(events_queue const&) = delete;
    events_queue& operator=(events_queue const&) = delete;
    
    events_queue();
    
    int add_event(const struct kevent& kev);
    int add_event(uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data, void* udata);
    int add_event(uintptr_t ident, int16_t filter, uint16_t flags, std::function<void(struct kevent&)> handler);
    int add_event(uintptr_t ident, int16_t filter, uint16_t flags, intptr_t data, std::function<void(struct kevent&)> handler);
    int add_event(uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data, std::function<void(struct kevent&)> handler);

    int delete_event(uintptr_t ident, int16_t filter);
    
    int occured();
    void execute();
    void invalidate_events(uintptr_t id);
    
    ~events_queue();
    
private:
    
    struct id {
        id();
        id(uintptr_t new_ident, int16_t new_filter);
        id(struct kevent const& event);
        
        friend bool operator==(id const&, id const&);
        friend bool operator<(id const&, id const&);
    private:
        uintptr_t ident;
        int16_t filter;
    };
    friend bool operator==(id const&, id const&);
    friend bool operator<(id const&, id const&);

    
    static const uint16_t EVLIST_SIZE = 512;
    int kq; // TODO: wrap in a RAII class
    struct kevent event_list[EVLIST_SIZE];
    std::set<uintptr_t> invalid;
    std::map<id, std::function<void(struct kevent&)> > handlers;
};

#endif /* event_queue_hpp */
