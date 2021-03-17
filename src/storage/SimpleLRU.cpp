#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
    size_t elem_size = key.size() + value.size();
    if (elem_size > _max_size)
        return false;
    
    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem == _lru_index.end())
        return PutIfAbsentElem(key, value);
    else
        return SetElem(key, value, &(elem->second.get()));
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) { 
    size_t elem_size = key.size() + value.size();
    if (elem_size > _max_size)
        return false;

    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem != _lru_index.end())
        return false;
    return PutIfAbsentElem(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) { 
    size_t elem_size = key.size() + value.size();
    if (elem_size > _max_size)
        return false;

    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem == _lru_index.end())
        return false;
    return SetElem(key, value, &(elem->second.get()));
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) { 
    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem == _lru_index.end())
        return false;
    return DeleteElem(&(elem->second.get()));
}

bool SimpleLRU::PutIfAbsentElem(const std::string &key, const std::string &value) { 
    size_t elem_size = key.size() + value.size();
    while (_cur_size + elem_size > _max_size)
        this->DeleteElem(_lru_tail);
    lru_node* cur = new lru_node({key, value, nullptr, nullptr}); 
    lru_node* old = nullptr;
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
    _cur_size = _cur_size + key.size() + value.size();
    return true; 
}

bool SimpleLRU::SetElem(const std::string &key, const std::string &value, lru_node* elem) { 
    size_t elem_size = value.size();
    this->MoveElem(elem);
    while (_cur_size + elem_size - elem->value.size() > _max_size)
        this->DeleteElem(_lru_tail);
    elem->value = value;
    return true; 
}

bool SimpleLRU::DeleteElem(lru_node* cur) { 
    lru_node* next = cur->next.release();
    cur->next.reset();
    lru_node* old = cur->prev;
    cur->prev = nullptr;
    _cur_size = _cur_size - cur->key.size() - cur->value.size();
    _lru_index.erase(std::reference_wrapper<const std::string>(cur->key));
    if (old != nullptr)
        old->next.reset(next);
    else
        _lru_head.reset(next);
    if (next != nullptr)
        next->prev = old;
    else
        _lru_tail = old;
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) { 
    auto elem = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (elem == _lru_index.end()){
        return false;
    }
    lru_node* cur = &(elem->second.get());
    this->MoveElem(cur);
    value = cur->value;
    return true; 
}

void SimpleLRU::MoveElem(lru_node* cur) { 
    lru_node* old = cur->prev;
    if (old == nullptr)
        return;

    lru_node* next = cur->next.release();
    cur->next.reset();
    cur->prev = nullptr;
    old->next.release();
    old->next.reset(next);
    if (next != nullptr)
        next->prev = old;
    else
        _lru_tail = old;

    old = _lru_head.release();
    _lru_head.reset(cur);
    old->prev = cur;
    cur->next.reset(old);
}

} // namespace Backend
} // namespace Afina
