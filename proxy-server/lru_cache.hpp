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

template<typename key, typename value>
struct lru_cache {
    lru_cache(size_t cache_size = 20000) :
        capacity(cache_size)
    {}
    
    void insert(const key& key_val, const value& val) {
        if (q.size() == capacity) {
            key tmp = q.front();
            q.pop();
            data.erase(tmp);
        }
        data[key_val] = val;
        q.push(key_val);
    }
    
    bool exists(const key& key_val) {
        return data.find(key_val) != data.end();
    }
    
    value operator[](const key& key_val) {
        return data[key_val];
    }
    
    size_t size() const noexcept {
        return size;
    }
    
private:
    size_t capacity;
    std::queue<key> q;
    std::map<key, value> data;
};


#endif /* lru_cache_hpp */
