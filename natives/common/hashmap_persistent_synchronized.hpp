#ifndef HASHMAP_PERSISTENT_SYNCHRONIZED_H
#define HASHMAP_PERSISTENT_SYNCHRONIZED_H

#include "hashmap_persistent.hpp"

#include <mutex>
#include <shared_mutex>

/**
 * WARNING: superclass intentionally does not have virtual methods
 * 
 * WARNING: iterating over / transactions require manual lockUnique / lockShared
 * 
 * WARNING: it is compulsory to call resetLock on opening an existing memory pool
 **/
template <typename K, typename V, class Hash = std::hash<K>, class Compare = std::equal_to<K>>
class hashmap_persistent_synchronized : public hashmap_persistent<K,V,Hash,Compare> {

    mutable std::shared_mutex mutex;

public:
    hashmap_persistent_synchronized(pmem::obj::pool_base & pop, size_t bucketCount = 128)
        : hashmap_persistent<K,V,Hash,Compare> (pop, bucketCount) {}

    hashmap_persistent_synchronized & operator=(const hashmap_persistent_synchronized &) = delete;

    template<typename KK, typename... ConstructorArgs>
    V& get (pmem::obj::pool_base & pop, const KK & k, ConstructorArgs... constructorArgs) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return hashmap_persistent<K,V,Hash,Compare>::get(pop, k, constructorArgs...);
    }

    template<typename KK>
    V * get_if_exists (const KK & k) {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return hashmap_persistent<K,V,Hash,Compare>::get_if_exists(k);
    }

    template<typename KK>
    const V * get_if_exists (const KK & k) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return hashmap_persistent<K,V,Hash,Compare>::get_if_exists(k);
    }

    template<typename KK>
    bool erase(pmem::obj::pool_base & pop, const KK & k) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return hashmap_persistent<K,V,Hash,Compare>::erase(pop, k);
    }

    void clear(pmem::obj::pool_base & pop) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        hashmap_persistent<K,V,Hash,Compare>::clear(pop);
    }

    void resetLock(){
        new (&mutex) std::shared_mutex();
    }
    
    std::unique_lock<std::shared_mutex> lockUnique(){
        return std::unique_lock<std::shared_mutex>(mutex);
    }

    std::shared_lock<std::shared_mutex> lockShared() const {
        return std::shared_lock<std::shared_mutex>(mutex);
    }

};

#endif // HASHMAP_PERSISTENT_SYNCHRONIZED_H



