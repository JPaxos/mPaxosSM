#pragma once
#include <cassert>
#include <type_traits>
#include <libpmemobj++/experimental/radix_tree.hpp>
#include <libpmemobj++/make_persistent_array.hpp>


namespace pmx = pmem::obj::experimental;
namespace pm = pmem::obj;

template <bool accounting>
struct AccountingData {
};

template <>
struct AccountingData<true> {
    size_t _totalBlocks{0};
    size_t _totalSize{0};
};

/**
 * This class is intended to keep track of unused memory blocks rather than
 * delete them. It store at most one block of each size (and deallocates any
 * of the same size once it gets pushed).
 * 
 * WARNING: The only allowed use of this class per a PMDK transacion is "take
 * a block and/or give a block" for each block size, in that order.
 **/

template <typename PtrType, bool accounting = false> 
    requires std::is_constructible_v<PtrType, std::nullptr_t>
class BlockReuser : protected AccountingData<accounting>{
  public: 
    PtrType pop(size_t size);
    void push(PtrType & block, size_t size);
    void push(PtrType && block, size_t size){push(block, size);}
    PtrType exchange(PtrType & block, size_t size);
    PtrType exchange(PtrType && block, size_t size){return exchange(block, size);}
    
    template <typename = void> requires accounting
    size_t totalBlocks() const {return this->_totalBlocks;}
    
    template <typename = void> requires accounting
    size_t totalSize() const {return this->_totalSize;}
  private:
    pmx::radix_tree<size_t, PtrType> _blockMap;
};


template <typename PtrType, bool accounting>
PtrType BlockReuser<PtrType,accounting>::pop(size_t size){
    auto it = _blockMap.find(size);
    if(it==_blockMap.end()){
        return nullptr;
    } else {
        assert(pmemobj_tx_stage() == TX_STAGE_WORK);
        PtrType ptr(std::move(it->value()));
        _blockMap.erase(it);
        if constexpr (accounting){
            this->_totalBlocks--;
            this->_totalSize-= size;
        }
        return ptr;
    }
}

template <typename PtrType, bool accounting>
void BlockReuser<PtrType,accounting>::push(PtrType & ptr, size_t size){
    assert(pmemobj_tx_stage() == TX_STAGE_WORK);
    auto &&[it, success] = _blockMap.try_emplace(size, ptr);
    if(!success){
        pm::delete_persistent<char[]>(ptr, size);
        return;
    }
    if constexpr (accounting){
        this->_totalBlocks++;
        this->_totalSize+= size;
    }
}

template <typename PtrType, bool accounting>
PtrType BlockReuser<PtrType,accounting>::exchange(PtrType & newPtr, size_t size){
    assert(pmemobj_tx_stage() == TX_STAGE_WORK);
    auto it = _blockMap.find(size);
    if(it==_blockMap.end()){
        _blockMap.try_emplace(size, std::move(newPtr));
        return nullptr;
    } else {
        PtrType ptr(std::move(it->value()));
        it->value() = std::move(newPtr);
        if constexpr (accounting){
            this->_totalBlocks--;
            this->_totalSize-= size;
        }
        return ptr;
    }
}
