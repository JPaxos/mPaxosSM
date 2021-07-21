#ifndef HASHSET_PERSISTENT_H
#define HASHSET_PERSISTENT_H

#include <cassert>

#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>


/**
 * This is a simple hash set with constant bucket size and place for an entry 
 * in each bucket head.
 * 
 * WARNING: if synchronized is true, then it is compulsory to call resetLock on
 *          opening an existing memory pool
 **/
template <typename E, bool synchronized = false, class Hash = std::hash<E>, class Compare = std::equal_to<E>>
class hashset_persistent{
    Compare _compare;
    Hash    _hash;
    
    // // // // // // // // // //
    template<typename T, bool exists = false>
    struct templateDependantField {};
    template<typename T>
    struct templateDependantField<T, true> {
        T t;
        template<typename ... Args> templateDependantField(Args... args) : t(std::forward(args)...){}
    };
    mutable templateDependantField<std::shared_mutex,synchronized> _mutex;
    // // // // // // // // // //
    
    struct NonHead {
        NonHead() = default;
        NonHead(const E & e){element = e;}
        pmem::obj::p<E> element;
        pmem::obj::persistent_ptr<NonHead> next = nullptr;
    };
    struct Head : public NonHead {
        pmem::obj::p<bool> valid = false;
    };
    
    pmem::obj::p<int> _bucketCount;
    pmem::obj::persistent_ptr<Head[]> _buckets;
    
    pmem::obj::p<size_t> _elementCount {0};

    inline Head* getBucketHeadFor(const E & element) const {
        return &(_buckets[_hash(element)%_bucketCount]);
    }
    
public:
    hashset_persistent(pmem::obj::pool_base & pop, int bucketCount) : _bucketCount(bucketCount) {
        pmem::obj::transaction::automatic tx(pop);
        _buckets = pmem::obj::make_persistent<Head[]>(bucketCount);
    }
    
    template<typename = std::enable_if<synchronized>>
    void resetLock(){
        new (&_mutex.t) std::shared_mutex();
    }
    
    template<typename = std::enable_if<synchronized>>
    std::unique_lock<std::shared_mutex> lockUnique(){
        return std::unique_lock<std::shared_mutex>(_mutex.t);
    }

    template<typename = std::enable_if<synchronized>>
    std::shared_lock<std::shared_mutex> lockShared() const {
        return std::shared_lock<std::shared_mutex>(_mutex.t);
    }
    
    bool add(pmem::obj::pool_base & pop, const E & element) {
        Head * bucketHead = getBucketHeadFor(element);
        
        std::unique_lock<std::shared_mutex> lock;
        if constexpr (synchronized)
            lock = std::unique_lock(_mutex.t);
        
        // empty head
        if(!bucketHead->valid){
            pmem::obj::transaction::automatic tx(pop);
            bucketHead->element = element;
            bucketHead->valid = true;
            _elementCount++;
            assert(bucketHead->next == nullptr);
            return true;
        }
        
        // nonempty head; reach the end of the linked list unless the element is found 
        NonHead * curr = bucketHead;
        while(true){
            // element exists
            if(_compare(element, curr->element))
                return false;
            // this is the last element
            if(curr->next==nullptr)
                break;
            curr = curr->next.get();
        }
        
        // append to tail
        pmem::obj::transaction::automatic tx(pop);
        curr->next = pmem::obj::make_persistent<NonHead>(element);
        _elementCount++;
        return true;
    }
        
    
    bool contains(const E & element) const {
        Head * bucketHead = getBucketHeadFor(element);
        
        std::shared_lock<std::shared_mutex> lock;
        if constexpr (synchronized)
            lock = std::shared_lock(_mutex.t);
        
        if(!bucketHead->valid){
            // no elements
            return false;
        }
        
        // traverse all elements, starting from head
        NonHead * i = bucketHead;
        do {
            if(_compare(i->element, element))
                return true;
        } while((i = i->next.get()) != nullptr);
        
        return false;
    }
    
    bool erase(pmem::obj::pool_base & pop, const E & element){
        Head * bucketHead = getBucketHeadFor(element);
        if(!bucketHead->valid){
            return false;
        }
        
        std::unique_lock<std::shared_mutex> lock;
        if constexpr (synchronized)
            lock = std::unique_lock(_mutex.t);
        
        // remove from head
        if(_compare(bucketHead->element, element)){
            if(bucketHead->next == nullptr){
            // there is only one element 
                pmem::obj::transaction::automatic tx(pop);
                bucketHead->valid = false;
                _elementCount--;
            } else {
            // there are elements past the head;
                pmem::obj::transaction::automatic tx(pop);
                auto old = bucketHead->next;
                bucketHead->element = old->element;
                bucketHead->next = old->next;
                pmem::obj::delete_persistent<NonHead>(old); 
                _elementCount--;
            }            
            return true;
        }
        
        // look up the element in the linked list or reach the end of the list
        NonHead * prev = bucketHead;
        while(1) {
            pmem::obj::persistent_ptr<NonHead> & curr = prev->next;
            if(curr == nullptr)
                return false;
            if(_compare(curr->element, element)){
                pmem::obj::transaction::automatic tx(pop);
                auto currCopy = curr;
                prev->next = curr->next;
                pmem::obj::delete_persistent<NonHead>(currCopy); 
                _elementCount--;
                return true;
            }
            prev = curr.get();
        }
    }
    
    size_t count() const {return _elementCount;}
    
private:
    class Iterator {
        friend class hashset_persistent<E, synchronized, Hash, Compare>;
        Iterator(const hashset_persistent<E, synchronized, Hash, Compare> * thisSet) : thisSet(thisSet) {}
        
        const hashset_persistent<E, synchronized, Hash, Compare> * thisSet;
        
        int nextBucket = 0;
        NonHead * currentElement = nullptr;
        
        Iterator & getFromNextBucket(){
            while(true){
                // return null if 'nextBucket' is past the buckets
                if(nextBucket == thisSet->_bucketCount){
                    nextBucket = 0;
                    currentElement = nullptr;
                    return *this;
                }
                
                // return first element of 'nextBucket'
                if(thisSet->_buckets[nextBucket].valid){
                    currentElement = &(thisSet->_buckets[nextBucket]);
                    return *this;
                }
                
                // go to next bucket
                ++nextBucket;
            }
        }
    public:
        Iterator & operator++(){
            // check if there are any elements left in this bucket
            if(currentElement->next != nullptr) {
                currentElement = currentElement->next.get();
                return *this;
            }
            // take the first element from the next nonempty bucket
            ++nextBucket;
            return getFromNextBucket();
        }
        const E & operator*(){
            return currentElement->element.get_ro();
        }
        bool operator!=(const Iterator& o){
            return o.currentElement != currentElement;
        }
    }; // end of class iterator
    
public:
    Iterator begin() const {
        auto it = Iterator(this);
        it.getFromNextBucket();
        return it;
    }
    
    Iterator end() const{
        return Iterator(this);
    }
};


#endif // HASHSET_PERSISTENT_H
