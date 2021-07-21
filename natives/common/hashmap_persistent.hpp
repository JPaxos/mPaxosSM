#ifndef HASHMAP_PERSISTENT_H
#define HASHMAP_PERSISTENT_H

#include <libpmem.h>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/experimental/self_relative_ptr.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/allocator.hpp>

#undef DETECT_CONCURRENT_ACCESS
#if defined(DETECT_CONCURRENT_ACCESS) && !defined(NDEBUG)
    #include <atomic>
    #define CHECK_INIT std::atomic<bool> _modificationInProgress {false};\
                       mutable std::atomic<bool> _iterating {false};\
                       mutable std::atomic<int> _readerCount {0};
    #define CHECK_ON_MODIFY_INVOKE _iterating = false;\
                                   assert(!_modificationInProgress.exchange(true));\
                                   assert(!_readerCount);
    #define CHECK_ON_MODIFY_RETURN assert(!_iterating);\
                                   assert(!_readerCount);\
                                   _modificationInProgress = false;
    #define CHECK_ON_READ_INVOKE   _readerCount++; assert(!_modificationInProgress);
    #define CHECK_ON_READ_RETURN   assert(!_modificationInProgress); _readerCount--;
    #define CHECK_ON_ITERATOR_INIT assert(!_modificationInProgress); _iterating = true;
    #define CHECK_ON_ITERATE       assert(_this->_iterating.exchange(true));
    /*
     * Separate things are checked:
     *  • _modificationInProgress - true while modificating, if true upon invoking modification, then fail
     *  • _readerCount - reader: inc cm? ... cm? dec
     *                   writer: cm readers?  ...  readers? !cm
     *  • _iterating - set to true when an iterator is created, checked if true when an iterator advances,
     *                 set to false when modification starts, checked if false when modification ends
     */
#else
    #define CHECK_INIT
    #define CHECK_ON_MODIFY_INVOKE
    #define CHECK_ON_MODIFY_RETURN
    #define CHECK_ON_READ_INVOKE 
    #define CHECK_ON_READ_RETURN
    #define CHECK_ON_ITERATOR_INIT 
    #define CHECK_ON_ITERATE
#endif

/**
 * This is a simple hash map with constant bucket size and place for a key and
 * a value in each bucket head. See ../service/NvmHashMap.hpp for a more
 * sophisticated implementation.
 **/

template <typename K, typename V, class Hash = std::hash<K>, class Compare = std::equal_to<K>>
class hashmap_persistent {
    struct bucket_entry {
        bucket_entry() = default;
        bucket_entry(K k) : key(k), value(V()) {}
        template<typename KK, typename... Args>
        bucket_entry(const KK & k, Args... args) : key(k), value(args...) {}
        template<typename KK, typename... Args>
        bucket_entry(KK && k, Args... args) : key(k), value(args...) {}
        K key;
        V value;
        pmem::obj::experimental::self_relative_ptr<bucket_entry> next {pmem::obj::experimental::self_relative_ptr<bucket_entry>()};
    };
    struct bucket_entry_head : public bucket_entry {
        // Warning: PMDK is able only to call the default constructor(!) for this class, since it is a part of an array.
        bucket_entry_head() = default;
        pmem::obj::p<bool> valid {false};
    };
    
    Hash    _hash;
    Compare _compare;
    pmem::obj::experimental::self_relative_ptr<bucket_entry_head[]> _buckets;
    pmem::obj::p<size_t> _bucketCount;
    pmem::obj::p<size_t> _elementCount {0};
    
    CHECK_INIT
    
    template<typename KK>
    bucket_entry_head * getBucketHead(const KK & k) const{
        return &(_buckets[_hash(k)%_bucketCount]);
    }
    
public:
    hashmap_persistent(pmem::obj::pool_base & pop, size_t bucketCount = 128) : _bucketCount(bucketCount) {
        pmem::obj::transaction::automatic tx(pop);
        _buckets = pmem::obj::make_persistent<bucket_entry_head[]>(bucketCount);
    }
    
    hashmap_persistent & operator=(const hashmap_persistent &) = delete;
    
    template<typename KK, typename... ConstructorArgs>
    V& get (pmem::obj::pool_base & pop, const KK & k, ConstructorArgs... constructorArgs){
        CHECK_ON_MODIFY_INVOKE
        bucket_entry_head * head = getBucketHead(k);
        
        // if there is no head, insert as head
        if(!head->valid){
            pmem::obj::transaction::manual tx(pop);
                // no need to snapshot key and value, as head is not valid, so
                // key and value are scratch
                new (&head->key) K(k);
                new (&head->value) V(constructorArgs...);
                head->valid = true;
                _elementCount++;
                pmem_flush(&head->key, sizeof(head->key));
                pmem_flush(&head->value, sizeof(head->value));
                pmem_drain();
            pmem::obj::transaction::commit();
            assert(head->next==nullptr);
            CHECK_ON_MODIFY_RETURN
            return head->value;
        }
        
        // look up and return, if not found, then create a new element
        bucket_entry * be = head;
        while(!_compare(be->key, k)){
            if(be->next == nullptr){
                pmem::obj::transaction::manual tx(pop);
                    be->next = pmem::obj::make_persistent<bucket_entry>(k, constructorArgs...);
                    _elementCount++;
                pmem::obj::transaction::commit();
                
                CHECK_ON_MODIFY_RETURN
                return be->next->value;
            }
            be = be->next.get();
        }
        CHECK_ON_MODIFY_RETURN
        return be->value;
    }
    
    template <typename KK>
    V* get_if_exists (const KK & k) {
        return get_if_exists_(k);
    }
    
    template <typename KK>
    const V* get_if_exists (const KK & k) const {
        return get_if_exists_(k);
    }
    
    private:
    template <typename KK>
    V* get_if_exists_ (const KK & k) const {
        CHECK_ON_READ_INVOKE
        bucket_entry_head * head = getBucketHead(k);
        
        if(!head->valid){
            CHECK_ON_READ_RETURN
            return nullptr;
        }
        
        bucket_entry * be = head;
        while(!_compare(be->key, k)){
            if(be->next == nullptr){
                CHECK_ON_READ_RETURN
                return nullptr;
            }
            be = be->next.get();
        }
        CHECK_ON_READ_RETURN
        return &be->value;
    }
    public:
    
    template <typename KK>
    bool erase(pmem::obj::pool_base & pop, const KK & k){
        CHECK_ON_MODIFY_INVOKE
        bucket_entry_head * head = getBucketHead(k);
        
        // no elements
        if(!head->valid)
            return false;
        
        // chopping head off
        if(_compare(head->key, k)){
            pmem::obj::transaction::manual tx(pop);
                if(head->next == nullptr){
                    head->valid = false;
                } else {
                    auto next = head->next;
                    pmem::obj::transaction::snapshot(&head->key);
                    pmem::obj::transaction::snapshot(&head->value);
                    // move may hurt old locations, hence we also need to
                    pmem::obj::transaction::snapshot(&next->key);
                    pmem::obj::transaction::snapshot(&next->value);
                    head->key = std::move(next->key);
                    head->value = std::move(next->value);
                    head->next = next->next;
                    pmem::obj::delete_persistent<bucket_entry>(next);
                }
                _elementCount--;
            pmem::obj::transaction::commit();
            CHECK_ON_MODIFY_RETURN
            return true;
        }
        
        // removing from tail 
        bucket_entry * be = head;
        while(1) {
            if(be->next == nullptr){
                CHECK_ON_MODIFY_RETURN
                return false;
            }
        
            if(_compare(be->next->key, k)){
                pmem::obj::transaction::manual tx(pop);
                    auto next = be->next;
                    be->next = next->next;
                    pmem::obj::delete_persistent<bucket_entry>(next);
                    _elementCount--;
                pmem::obj::transaction::commit();
                CHECK_ON_MODIFY_RETURN
                return true;
            }
            be = be->next.get();
        }
    }
    
    void clear(pmem::obj::pool_base & pop) {
        CHECK_ON_MODIFY_INVOKE
        pmem::obj::transaction::manual tx(pop);
            for(size_t i = 0; i < _bucketCount; ++i){
                auto & head = _buckets[i];
                head.valid = false;
                decltype(head.next) & curr = head.next, next;
                
                while(curr) {
                    next = curr->next;
                    pmem::obj::delete_persistent<bucket_entry>(curr);
                    curr = next;
                }
            }
            _elementCount = 0;
        pmem::obj::transaction::commit();
        CHECK_ON_MODIFY_RETURN
    }
    
    size_t count() const {return _elementCount;}
    
private:
    /// Iterator is NOT persitable
    class iterator_base {
    protected:
        friend class hashmap_persistent<K, V, Hash, Compare>;
        iterator_base(const hashmap_persistent * hm): _this(hm){}
        bucket_entry * thisItem = nullptr;
        size_t nextBucket = 0;
        const hashmap_persistent * _this;
        iterator_base& getFromNextBucket(){
            while(1) {
                auto * nextBucketHead = &(_this->_buckets[nextBucket]);
                if(!nextBucketHead->valid){
                    nextBucket++;
                    if(nextBucket < _this->_bucketCount)
                        continue;
                    
                    nextBucket = 0;
                    thisItem = nullptr;
                    return *this;
                }
                thisItem = nextBucketHead;
                return *this;
            }
        }
    public:
        iterator_base& operator++(){
            CHECK_ON_ITERATE
            if(thisItem->next!=nullptr){
                thisItem = thisItem->next.get();
                return *this;
            }
            ++nextBucket;
            if(nextBucket == _this->_bucketCount){
                thisItem = nullptr;
                nextBucket = 0;
                return *this;
            }
            return getFromNextBucket();
        }
        
        bool operator!=(const iterator_base& o) const {
            return o.thisItem != thisItem;
        }
    };
    
    class const_iterator : public iterator_base {
    public:
        const_iterator(const hashmap_persistent * hm) : iterator_base(hm){}
        const std::pair<const K&, const V&> operator*() const {
            return std::pair<const K&, const V&>(this->thisItem->key, this->thisItem->value);
        }
    };
    
    class iterator : public const_iterator {
    public:
        iterator(const hashmap_persistent * hm) : const_iterator(hm){}
        std::pair<const K&, V&> operator*(){
            if (TX_STAGE_WORK == pmemobj_tx_stage())
                pmem::obj::transaction::snapshot(&this->thisItem->value);
            return std::pair<const K&, V&>(this->thisItem->key, this->thisItem->value);
        }
    };
    

    
public:
    iterator begin(){
        iterator it(this);
        CHECK_ON_ITERATOR_INIT
        it.getFromNextBucket();
        return it;
    }
    
    iterator end(){
        return iterator(this);
    }
    
    const_iterator begin() const {
        const_iterator it(this);
        CHECK_ON_ITERATOR_INIT
        it.getFromNextBucket();
        return it;
    }
    
    const_iterator end() const {
        return const_iterator(this);
    }
};

#endif // HASHMAP_PERSISTENT_H



