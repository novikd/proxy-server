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
#include <list>

//FIXME: It works in the wrong way
//       enqueue for existing element can break everything

template<typename key_t, typename value_t>
struct lru_cache {
    lru_cache(size_t cache_size = 20000) :
        capacity(cache_size)
    {}
    
    void insert(const key_t& key, const value_t& value) {
        if (list.size() == capacity) {
            if (exists(key)) {
                for (auto it = list.begin(); it != list.end(); ++it) {
                    if (*it == key) {
                        list.erase(it);
                        break;
                    }
                }
            } else {
                key_t tmp = list.front();
                list.pop_front();
                data.erase(tmp);
            }
        }
        data[key] = value;
        list.push_back(key);
    }
    
    bool exists(const key_t& key) {
        return data.find(key) != data.end();
    }
    
    value_t operator[](const key_t& key) {
        return data[key];
    }
    
    size_t size() const noexcept {
        return list.size();
    }
    
private:
    size_t capacity;
    std::list<key_t> list;
    std::map<key_t, value_t> data;
};


#endif /* lru_cache_hpp */
