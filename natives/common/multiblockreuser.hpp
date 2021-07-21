#pragma once
#include <cassert>
#include <type_traits>
#include <libpmemobj++/experimental/radix_tree.hpp>
#include <libpmemobj++/make_persistent_array.hpp>

#include "naive_persistent_stack.hpp"

#include <typeinfo> 

#ifdef __GNUG__
#include <cxxabi.h>
#endif

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
 * MultiBlockReuser is intended to reclaim pmem memory blocks so they can be
 * later retreived and used as a scratch pad - one can write the popped /
 * exchanged block and persist it without the need to snapshot it beforehand.
 * This is much faster than delete_persistent and make_persistent, but the
 * class just stores the blocks as passed to it, so it works reasonably only
 * when the application needs to use (with some restriction, described below)
 * blocks of repetitive sizes. Otherwise memory consumption explodes.
 * 
 * WARNING: MultiBlockReuser has several limitations:
 *   1. Non-const methods must be called from within a PMDK transaction
 *   2. In one PMDK transaction one can call, for each size separatly,
 *      arbitrary number of pop operations followed by at most one exchange, 
 *      or at most one exchange followed by an arbitrary number of pushes.
 *      Any other combination is incorrect, as the MultiBlockReuser will return
 *      a block pused / exchanged in this transaction, and so it would have to
 *      be snapshotted.
 *   3. This class is not thread safe.
 *   4. (Obvoiously) PtrType must be a persistent pointer type
 **/
template <typename PtrType, bool accounting = false> 
    requires std::is_constructible_v<PtrType, std::nullptr_t>
class MultiBlockReuser : protected AccountingData<accounting>{
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
    
    void dump(FILE* out) const;
    
  private:
    pmx::radix_tree<size_t, naive_persistent_stack<PtrType>> _blockMap;
};


template <typename PtrType, bool accounting>
PtrType MultiBlockReuser<PtrType,accounting>::pop(size_t size){
    auto it = _blockMap.find(size);
    if(it==_blockMap.end()){
        return nullptr;
    } else {
        auto & l = it->value();
        if(l.empty())
            return nullptr;
        assert(pmemobj_tx_stage() == TX_STAGE_WORK);
        PtrType ptr(l.back());
        l.pop_back();
        if constexpr (accounting){
            this->_totalBlocks--;
            this->_totalSize-= size;
        }
        return ptr;
    }
}

template <typename PtrType, bool accounting>
void MultiBlockReuser<PtrType,accounting>::push(PtrType & ptr, size_t size){
    assert(pmemobj_tx_stage() == TX_STAGE_WORK);
    auto &&[it, success] = _blockMap.try_emplace(size);
    auto & l = it->value();
    l.push_back(std::move(ptr));
    if constexpr (accounting){
        this->_totalBlocks++;
        this->_totalSize+= size;
    }
}

template <typename PtrType, bool accounting>
PtrType MultiBlockReuser<PtrType,accounting>::exchange(PtrType & newPtr, size_t size){
    assert(pmemobj_tx_stage() == TX_STAGE_WORK);
    auto &&[it, success] = _blockMap.try_emplace(size);
    auto & l = it->value();
    if(l.empty()){
        l.push_back(std::move(newPtr));
        return nullptr;
    } else {
        PtrType ptr(std::move(l.back()));
        l.back() = std::move(newPtr);
        if constexpr (accounting){
            this->_totalBlocks--;
            this->_totalSize-= size;
        }
        return ptr;
    }
}

template <typename PtrType, bool accounting>
void MultiBlockReuser<PtrType,accounting>::dump(FILE* out) const {
    #ifdef __GNUG__
    int status;
    char * demangledType = abi::__cxa_demangle(typeid(PtrType).name(), nullptr, nullptr, &status);
    if(!status){
        fprintf(out, "MultiBlockReuser of %s: \n", demangledType);
        free(demangledType);
    } else
    #endif
    fprintf(out, "MultiBlockReuser of %s: \n", typeid(PtrType).name());

    for(const auto &leaf : _blockMap){
        fprintf(out, "%12zu: %zu\n", leaf.key(), leaf.value().size());
    }
    if constexpr(accounting){
        fprintf(out, "  %zu blocks, %zu bytes total.\n", this->_totalBlocks, this->_totalSize);
    } 
}
