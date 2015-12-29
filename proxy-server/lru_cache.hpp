//
//  lru_cache.hpp
//  proxy-server
//
//  Created by Дмитрий Новик on 24.12.15.
//  Copyright © 2015 Дмитрий Новик. All rights reserved.
//

#ifndef lru_cache_hpp
#define lru_cache_hpp

#include <map>
#include <queue>

//FIXME: It works in the wrong way
//       enqueue for existing element can break everything

template<typename key_t, typename value_t>
struct lru_cache {
    lru_cache(size_t cache_size = 20000) :
        capacity(cache_size)
    {}
    
    void insert(const key_t& key, const value_t& value) {
        if (q.size() == capacity) {
            key_t tmp = q.front();
            q.pop();
            data.erase(tmp);
        }
        data[key] = value;
        q.push(key);
    }
    
    bool exists(const key_t& key) {
        return data.find(key) != data.end();
    }
    
    value_t operator[](const key_t& key) {
        return data[key];
    }
    
    size_t size() const noexcept {
        return size;
    }
    
private:
    size_t capacity;
    std::queue<key_t> q;
    std::map<key_t, value_t> data;
};


#endif /* lru_cache_hpp */
