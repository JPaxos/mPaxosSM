/**
 * This file is part of Bachelor Thesis
 *   NVM-enabled Distributed Hash Table system
 * by 
 *   Patryk Stasiewski, Kinga Neumann, Kamil Kołodziej, Miłosz Pogodski
 * defended at Poznan University of Technology in Match 2020
 * 
 * It has been improved by Jan Kończak in 2020 and 2021.
 * 
 * The code is licensed under LGPL 3.0
 */

#ifndef NVMHASHMAP_HPP_INCLUDED
#define NVMHASHMAP_HPP_INCLUDED

#include <iostream>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/utils.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/mutex.hpp>
#include <libpmemobj++/shared_mutex.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <unistd.h>
#include <memory>
#include <string.h>
#include <cmath>
#include <shared_mutex>
#include <concepts>

struct no_such_key_exception : public std::logic_error{
    no_such_key_exception(const std::string & what) : std::logic_error(what){}
    no_such_key_exception(const char* what = "no such key") : std::logic_error(what){}
};

template <typename K1, typename K2>
struct DefaultComparator { bool operator()(const K1 & k1, const K1 & k2){return k1 == k2;}};

template <typename X1, typename X2>
struct DefaultFactory {
    X1 operator()(const X2 & o){return o;}
    void operator()(X1 & d, const X2 & s){d=s;}
};

template<
    typename K,
    typename V,
    bool useLocks = true,
    typename Hash = std::hash<K>,
    typename Comparator = DefaultComparator<K, K>,
    typename KeyFactory = DefaultFactory<K, K>,
    typename ValueFactory = DefaultFactory<V, V>
    >
class NvmHashMap {
public:
    template<typename KK, typename VV, typename KKeyFactory, typename VValueFactory>
    class Iterator;
private:
    struct SegmentObject {
        pmem::obj::p <K> key;
        pmem::obj::p <V> value;
        pmem::obj::persistent_ptr <SegmentObject> next = nullptr;
    };

    struct Segment {
        pmem::obj::p<size_t> size = 0;
        pmem::obj::persistent_ptr <SegmentObject> head = nullptr;
        
        void deleteMemory(){
            auto element = head;
            while(element != nullptr) {
                auto prev = element;
                element = element->next;
                pmem::obj::delete_persistent<SegmentObject>(prev);
                prev = nullptr;
            }
        }
    };

    struct ArrayOfSegments {
        pmem::obj::p<size_t> arraySize;
        pmem::obj::persistent_ptr<Segment[]> segments;
        pmem::obj::p<size_t> elementsCount = 0;

        ArrayOfSegments() : ArrayOfSegments(16){}

        ArrayOfSegments(size_t arraySize) :
            arraySize(arraySize),
            segments(pmem::obj::make_persistent<Segment[]>(arraySize)){
        }
        
        ~ArrayOfSegments(){}

        void deleteMemoryShallow() {
            pmem::obj::delete_persistent<Segment[]>(segments, arraySize);
            segments = nullptr;
        }
        
        void deleteMemory(){
            for(size_t i = 0; i < arraySize; ++i)
                segments[i].deleteMemory();
            deleteMemoryShallow();
        }
    };

private:    
    const size_t _internalMapsCount;
    pmem::obj::persistent_ptr <ArrayOfSegments[]> _arrayOfSegments;
    pmem::obj::persistent_ptr <pmem::obj::shared_mutex[]> _arrayOfMutex;

    Hash _hash;
    
    template<typename KK, typename... VV>
    bool insertOrReplaceIntoInternalArray(pmem::obj::pool_base & pop, const KK & key, ArrayOfSegments& aos, const VV&... value) {
        size_t hash = _hash(key);
        hash = hash >> ((int)std::log2(_internalMapsCount));

        size_t index2 = hash % aos.arraySize;

        pmem::obj::persistent_ptr <SegmentObject> ptr = aos.segments[index2].head;

        if (ptr == nullptr) { // empty list
            pmem::obj::transaction::run(pop, [&] {
                ptr = pmem::obj::make_persistent<SegmentObject>();
                KeyFactory()(ptr->key.get_rw(), key);
                ValueFactory()(ptr->value.get_rw(), value...);
                aos.segments[index2].head = ptr;
                aos.segments[index2].size.get_rw()++;
                aos.elementsCount.get_rw()++;
            });
            return true;
        }
        
        while (true) {
            if (Comparator()(ptr->key.get_ro(), key)) {
                pmem::obj::transaction::run(pop, [&] {
                    ValueFactory()(ptr->value.get_rw(), value...);
                });
                return false;
            }
            if (ptr->next == nullptr) { // it's the last item of the list
                pmem::obj::transaction::run(pop, [&] {
                    ptr->next = pmem::obj::make_persistent<SegmentObject>();
                    KeyFactory()(ptr->next->key.get_rw(), key);
                    ValueFactory()(ptr->next->value.get_rw(), value...);
                    aos.segments[index2].size.get_rw()++;
                    aos.elementsCount.get_rw()++;
                });
                return true;
            }
            ptr = ptr->next;
        }
    }

    size_t getNumberOfInsertedElements(size_t arrayIndex) {
        size_t size = 0;

        for (size_t i = 0; i < _arrayOfSegments[arrayIndex].arraySize; i++)
            size += _arrayOfSegments[arrayIndex].segments[i].size;
        
        return size;
    }
    
public:
    NvmHashMap(size_t internalMapsCount = 8) : _internalMapsCount(internalMapsCount) {
        if (internalMapsCount <= 0)
            throw new std::invalid_argument("internalMapsCount must be a power of two");
            
        if((internalMapsCount & (internalMapsCount -1)) != 0)
            internalMapsCount = pow(2, std::ceil(std::log2(internalMapsCount)));
        
        _arrayOfSegments = pmem::obj::make_persistent<ArrayOfSegments[]>(_internalMapsCount);
        if constexpr (useLocks)
            _arrayOfMutex = pmem::obj::make_persistent<pmem::obj::shared_mutex[]>(_internalMapsCount);
    }

    void deleteMemory(){
        auto pop = pmem::obj::pool_by_vptr(this);
        return deleteMemory(pop);
    }
    
    void deleteMemory(pmem::obj::pool_base & pop) {
        pmem::obj::transaction::run(pop,[&]{
            for(size_t i = 0 ; i < _internalMapsCount; ++i)
                _arrayOfSegments[i].deleteMemory();
            pmem::obj::delete_persistent<ArrayOfSegments[]>(_arrayOfSegments, _internalMapsCount);
            if constexpr (useLocks)
                pmem::obj::delete_persistent<pmem::obj::shared_mutex[]>(_arrayOfMutex, _internalMapsCount);
        });
    }
    
    template<typename KK, typename... VV>
        requires (! std::derived_from<KK, pmem::obj::pool_base>)
    bool insertOrReplace(const KK & key, const VV&... value) {
        auto pop = pmem::obj::pool_by_vptr(this);
        return insertOrReplace(pop, key, value...);
    }
    
    /** returns true if this was an insert, false if it was a replace */
    template<typename KK, typename... VV>
    bool insertOrReplace(pmem::obj::pool_base & pop, const KK & key, const VV&... value) {
        size_t hash = _hash(key);
        size_t index = hash & (_internalMapsCount - 1);

        if constexpr (useLocks)
            _arrayOfMutex[index].lock();
        //(gdb) break NvmHashMap.hpp:166 if atoi(key)==85
        if (_arrayOfSegments[index].elementsCount > 0.7*_arrayOfSegments[index].arraySize) {
            expand(pop, index);
        }

        bool res = insertOrReplaceIntoInternalArray(pop, key, _arrayOfSegments[index], value...);
        
        if constexpr (useLocks)
            _arrayOfMutex[index].unlock();
        
        return res;
    }
    
    template<typename VV, typename KK, typename VValueFactory = ValueFactory>
    VV get(KK key) {
        size_t hash = _hash(key);
        size_t index = hash & (_internalMapsCount - 1);
        hash = hash >> ((int)std::log2(_internalMapsCount));
        size_t index2 = hash % _arrayOfSegments[index].arraySize;
        
        if constexpr (useLocks)
            _arrayOfMutex[index].lock_shared();
        
        pmem::obj::persistent_ptr <SegmentObject> ptr = _arrayOfSegments[index].segments[index2].head;

        if (ptr == nullptr) {
            if constexpr (useLocks)
                _arrayOfMutex[index].unlock_shared();
            throw no_such_key_exception();
        }
        
        while (true) {
            if (Comparator()(ptr->key.get_ro(), key)) {
                VV value = VValueFactory()(ptr->value.get_rw());
                if constexpr (useLocks)
                    _arrayOfMutex[index].unlock_shared();
                return value;
            }
            if (ptr->next == nullptr) {
                if constexpr (useLocks)
                    _arrayOfMutex[index].unlock_shared();
                throw no_such_key_exception();
            }
            ptr = ptr->next;   
        }
    }
    
    template<typename VV, typename KK, typename VValueFactory = ValueFactory>
    VV get(KK key) const {
        size_t hash = _hash(key);
        size_t index = hash & (_internalMapsCount - 1);
        hash = hash >> ((int)std::log2(_internalMapsCount));
        size_t index2 = hash % _arrayOfSegments[index].arraySize;
        
        if constexpr (useLocks)
            _arrayOfMutex[index].lock_shared();
        
        pmem::obj::persistent_ptr <SegmentObject> ptr = _arrayOfSegments[index].segments[index2].head;

        if (ptr == nullptr) {
            if constexpr (useLocks)
                _arrayOfMutex[index].unlock_shared();
            throw no_such_key_exception();
        }
        
        while (true) {
            if (Comparator()(ptr->key.get_ro(), key)) {
                VV value = VValueFactory()(ptr->value.get_ro());
                if constexpr (useLocks)
                    _arrayOfMutex[index].unlock_shared();
                return value;
            }
            if (ptr->next == nullptr) {
                if constexpr (useLocks)
                    _arrayOfMutex[index].unlock_shared();
                throw no_such_key_exception();
            }
            ptr = ptr->next;   
        }
    }

    template<typename VV, typename KK, typename VValueFactory = ValueFactory>
    VV remove(const KK & key) {
        auto pop = pmem::obj::pool_by_vptr(this);
        return remove<VV>(pop, key);
    }
    
    template<typename VV, typename KK, typename VValueFactory = ValueFactory>
    VV remove(pmem::obj::pool_base & pop, const KK & key) {
        size_t hash = _hash(key);
        size_t index = hash & (_internalMapsCount - 1);
        hash = hash >> ((int)std::log2(_internalMapsCount));
        size_t index2 = hash % _arrayOfSegments[index].arraySize;
        
        if constexpr (useLocks)
            _arrayOfMutex[index].lock();

        pmem::obj::persistent_ptr <SegmentObject> ptr = _arrayOfSegments[index].segments[index2].head;
        std::reference_wrapper<decltype(ptr)> toReplace = _arrayOfSegments[index].segments[index2].head;
        
        if (ptr == nullptr) {
            if constexpr (useLocks)
                _arrayOfMutex[index].unlock();
            throw no_such_key_exception();
        }
        
        while (true) {
            if (Comparator()(ptr->key.get_ro(), key)) {
                VV value = VValueFactory()(ptr->value.get_ro());
                pmem::obj::transaction::run(pop, [&] {
                    toReplace.get() = ptr->next;
                    pmem::obj::delete_persistent<SegmentObject>(ptr);
                    _arrayOfSegments[index].segments[index2].size.get_rw()--;
                    _arrayOfSegments[index].elementsCount.get_rw()--;
                });
                
                if constexpr (useLocks)
                    _arrayOfMutex[index].unlock();
                return value;
            }
            if (ptr->next == nullptr) {
                if constexpr (useLocks)
                    _arrayOfMutex[index].unlock();
                throw no_such_key_exception();
            }
            toReplace = ptr;
            ptr = ptr->next;
        }
    }

    void expand(pmem::obj::pool_base & pop, const int arrayIndex) {
        auto & aos = _arrayOfSegments[arrayIndex];
        int arraySize = aos.arraySize;
        
        pmem::obj::transaction::run(pop, [&] {
            int newArraySize = 4 * arraySize;
            ArrayOfSegments newAos(newArraySize);
            
            // bookkeep tails 
            decltype(aos.segments[0].head) * tails[newArraySize];
            for(int i = 0; i < newArraySize; i++)
                tails[i]=&(newAos.segments[i].head);
            
            // go through old segments
            for (int i = 0; i < arraySize; i++) {
                auto ptr = aos.segments[i].head;
                
                while (ptr != nullptr) {
                    //calculate new segment index
                    size_t hash = _hash(ptr->key.get_ro());
                    hash = hash >> ((int)std::log2(_internalMapsCount));
                    size_t index2 = hash % newArraySize;
                
                    // reattach to a new segment
                    *tails[index2] = ptr;
                    // iterate
                    ptr = ptr->next;
                    // clear next of the reattached element
                    (*tails[index2])->next = nullptr;
                    
                    // update tail
                    tails[index2] = &((*tails[index2])->next);
                    // keep count up-to-date
                    newAos.segments[index2].size.get_rw()++;
                }
            }
            _arrayOfSegments[arrayIndex].deleteMemoryShallow();
            _arrayOfSegments[arrayIndex] = newAos;
        });
    }

    size_t size() {
        size_t size = 0;

        for (size_t i = 0; i < _internalMapsCount; i++)
            size += getNumberOfInsertedElements(i);
        
        return size;
    }

    template<typename KK = K, typename VV = V, typename KKeyFactory = KeyFactory, typename VValueFactory = ValueFactory>
    const Iterator<KK, VV, KKeyFactory, VValueFactory> begin() const{
        return Iterator<KK, VV, KKeyFactory, VValueFactory>(this);
    }
    
    template<typename KK = K, typename VV = V, typename KKeyFactory = KeyFactory, typename VValueFactory = ValueFactory>
    const Iterator<KK, VV, KKeyFactory, VValueFactory> end() const{
        return Iterator<KK, VV, KKeyFactory, VValueFactory>();
    }
    
    void printInternalStructure(std::function<std::string(const K&)> keyToString = std::to_string, std::function<std::string(const V&)> valueToString = std::to_string) const {
        for(size_t i = 0 ; i < _internalMapsCount; ++i){
            std::cout << "ArrayOfSegments #" << i << std::endl;
            const auto & aos = _arrayOfSegments[i];
            for(size_t j = 0 ; j < aos.arraySize; ++j){
                std::cout << "  Segment #" << j << std::endl;
                auto element = aos.segments[j].head;
                while(element!=nullptr){
                    std::cout << "    " << keyToString(element->key.get_ro()) << " → " << valueToString(element->value.get_ro()) << std::endl;
                    element = element->next;
                }
            }
        }
    }

    template<typename KK, typename VV, typename KKeyFactory, typename VValueFactory>
    class Iterator {
        friend class NvmHashMap;
        
        const NvmHashMap * const map;
        
        int currentArrayIndex = 0;
        int currentSegmentIndex = 0;
        
        ArrayOfSegments *currentArray;
        Segment *currentSegment;
        SegmentObject* currentSegmentObject;
        
        std::shared_lock <pmem::obj::shared_mutex> * currLock = nullptr;
    protected:
        Iterator() : map(nullptr), currentArrayIndex(-1) {}
        
        Iterator(const Iterator& other) = delete;
        
        Iterator(const NvmHashMap * const map) : map(map) {
            if constexpr (useLocks)
                currLock = new std::shared_lock(map->_arrayOfMutex[currentArrayIndex]);
            currentArray = &(map->_arrayOfSegments[currentArrayIndex]);
            currentSegment = &(currentArray->segments[currentArrayIndex]);
            currentSegmentObject = currentSegment->head.get();
            if(currentSegmentObject == nullptr)
                ++(*this);
        }
        
    public:
        
        ~Iterator(){
            if constexpr (useLocks)
                delete currLock;
        }
        
        bool operator != (const Iterator & other) const {
            if(-1 == currentArrayIndex && -1 == other.currentArrayIndex)
                return false;
            if(currentArrayIndex != other.currentArrayIndex)
                return true;
            if(currentSegmentIndex != other.currentSegmentIndex)
                return true;
            if(currentSegmentObject != other.currentSegmentObject)
                return true;
            if(map != other.map)
                throw std::domain_error("Comparing iterators of different maps");
            return false;
        }
        
        Iterator & operator ++() {
            if(currentArrayIndex == -1)
                throw std::logic_error("Cannot go past end of a map");
                
            while (true) {
                outerloop:
                if (currentSegmentObject != nullptr && currentSegmentObject->next != nullptr) {
                // next object in this segment
                    currentSegmentObject = currentSegmentObject->next.get();
                    return *this;
                } else if ((size_t)currentSegmentIndex + 1 < currentArray->arraySize) {
                // next segment
                    do {
                        if((size_t)++currentSegmentIndex >= currentArray->arraySize)
                            goto outerloop;
                        currentSegment = &(currentArray->segments[currentSegmentIndex]);
                    } while (currentSegment->head == nullptr);
                    if (currentSegment->head != nullptr) {
                        currentSegmentObject = currentSegment->head.get();
                        return *this;
                    }
                    continue;
                } else if ((size_t) ++currentArrayIndex < map->_internalMapsCount) {
                // next array of segments
                    if constexpr (useLocks){
                        delete currLock;
                        currLock = new std::shared_lock(map->_arrayOfMutex[currentArrayIndex]);
                    }
                    currentArray = &(map->_arrayOfSegments[currentArrayIndex]);
                    currentSegmentIndex = 0;
                    currentSegment = &(currentArray->segments[0]);
                    
                    if (currentSegment->head != nullptr) {
                        currentSegmentObject = currentSegment->head.get();
                        return *this;
                    }
                    continue;
                } else {
                    if constexpr (useLocks){
                        delete currLock;
                        currLock = nullptr;
                    }
                    
                    currentArrayIndex = -1;
                    return *this;
                }
            }
        }
        
        std::pair<KK, VV> operator *() {
            return {KKeyFactory()(currentSegmentObject->key.get_ro()), VValueFactory()(currentSegmentObject->value.get_ro())};
        }
    };

};

#endif // NVMHASHMAP_HPP_INCLUDED
