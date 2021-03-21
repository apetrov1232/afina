#ifndef AFINA_STORAGE_STRIPED_LOCK_LRU_H
#define AFINA_STORAGE_STRIPED_LOCK_LRU_H

#include <afina/Storage.h>
#include "ThreadSafeSimpleLRU.h"
#include <vector>
namespace Afina {
namespace Backend {

int hash(std::string key)
{
  int len = key.size(), hashf = 0;
  if (len <= 1)
    hashf = key[0];
  else
    hashf = key[0] + key[len-1];

  return hashf;
};

class StripedLockLRU : public Afina::Storage{
public:
    StripedLockLRU(StripedLockLRU&&) = default;
    StripedLockLRU(size_t shards_cnt = 2, size_t max_size = 2*1024*1024) {
        shards.resize(shards_cnt);
        for (size_t i=0; i < shards_cnt; i++)
            shards[i] = std::unique_ptr<ThreadSafeSimplLRU>(new ThreadSafeSimplLRU(max_size/shards_cnt));;
    }

    static std::unique_ptr<StripedLockLRU> create_storage(size_t shards_cnt = 2, size_t max_size = 2*1024*1024) {
        if ((max_size / shards_cnt) < 1024*1024)
            throw std::runtime_error("error");
        else 
            return std::unique_ptr<StripedLockLRU>(new StripedLockLRU(shards_cnt, max_size));
    }

    ~StripedLockLRU() {}

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override{
        // TODO: sinchronization
        size_t k = hash(key) % shards.size();
        return shards[k]->Put(key, value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override{
        // TODO: sinchronization
        size_t k = hash(key) % shards.size();
        return shards[k]->PutIfAbsent(key, value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override{
        // TODO: sinchronization
        size_t k = hash(key) % shards.size();
        return shards[k]->Set(key, value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override{
        // TODO: sinchronization
        size_t k = hash(key) % shards.size();
        return shards[k]->Delete(key);
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override{
        // TODO: sinchronization
        size_t k = hash(key) % shards.size();
        return shards[k]->Get(key, value);
    }

private:
    // TODO: sinchronization primitives
    std::vector<std::unique_ptr<ThreadSafeSimplLRU> > shards;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_STRIPED_LOCK_LRU_H
