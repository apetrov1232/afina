#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
    this->Delete(key);
    return PutIfAbsent(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) { 
    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem != _lru_index.end())
        return false;
    size_t cur_size = key.size() + value.size();
    if (cur_size > _max_size)
        return false;
    
    lru_node* cur = new lru_node({key, value, nullptr, nullptr}); 
    lru_node* old;
    while (_cur_size + cur_size > _max_size)
        this->Delete(_lru_tail->key);
    if (_cur_size  == 0){
        _lru_head.reset(cur);
        _lru_tail = cur;
    }
    else {
        old = _lru_head.release();
        _lru_head.reset(cur);
        old->prev = cur;
        cur->next.reset(old);
    }
    _lru_index.emplace(std::reference_wrapper<const std::string>(cur->key), std::reference_wrapper<lru_node>(*cur));
    _cur_size = _cur_size + cur_size;
    cur = nullptr;
    old = nullptr;
    return true; 
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) { 
    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem == _lru_index.end())
        return false;
    lru_node* cur = &(elem->second.get());
    cur->value = value;
    return true; 
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) { 
    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem == _lru_index.end())
        return false;
    lru_node* cur = &(elem->second.get());
    lru_node* next = cur->next.release();
    cur->next.reset();
    lru_node* old = cur->prev;
    cur->prev = nullptr;
    _cur_size = _cur_size - cur->key.size() - cur->value.size();
    _lru_index.erase(std::reference_wrapper<const std::string>(key));
    if (old != nullptr)
        old->next.reset(next);
    else
        _lru_head.reset(next);
    if (next != nullptr)
        next->prev = old;
    else
        _lru_tail = old;
    cur = nullptr;
    next = nullptr;
    old = nullptr;
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) { 
    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem == _lru_index.end()){
        return false;
    }
    lru_node* cur = &(elem->second.get());
    value = cur->value;
    cur = nullptr;
    return true; 
}


} // namespace Backend
} // namespace Afina
