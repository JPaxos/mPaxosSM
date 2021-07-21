#pragma once

#include <libpmem.h>
#include <libpmemobj.h>

#include <libpmemobj++/experimental/self_relative_ptr.hpp>
#include <libpmemobj++/make_persistent.hpp>

#define CHECK_IN_TRANSACTION if (pmemobj_tx_stage() != TX_STAGE_WORK) throw pmem::transaction_scope_error("Operation executed outside transaction");

/**
 * WARNING: this stack is fragile:
 * 1) it must be used in PMDK transaction
 * 2) in one transaction one can either push or pop - mixing is not allowed
 */

template <typename E>
class naive_persistent_stack {
    using PersistentPtrType = pmem::obj::experimental::self_relative_ptr<E[]>;
  public:
    naive_persistent_stack(size_t initialCapacity = 32) : _size(0), _capacity(initialCapacity), _data(pmem::obj::make_persistent<E[]>(_capacity)) {}

    E &front() { return *_data.get(); }
    const E &front() const { return *_data.get(); }

    E &operator[](size_t pos) { return *(_data.get() + pos); }
    const E &operator[](size_t pos) const { return *(_data.get() + pos); }

    E &back() { return *(_data.get() + _size - 1); }
    const E &back() const { return *(_data.get() + _size - 1); }

    size_t size() const { return _size; }
    bool empty() const { return !_size; }

    template <typename... Args> void push_back(Args... args) {
        CHECK_IN_TRANSACTION
        if (_capacity == _size)
            extend();
        new (_data.get() + _size) E(std::forward<Args>(args)...);
        pmem_persist(_data.get() + _size, sizeof(E));
        _size++;
    }

    void pop_back() {
        CHECK_IN_TRANSACTION
        _size--;
    }

  protected:
    pmem::obj::p<size_t> _size;
    pmem::obj::p<size_t> _capacity;
    PersistentPtrType _data;
    
    void extend() {
        _capacity *= 2;
        PersistentPtrType newData = pmem::obj::make_persistent<E[]>(_capacity);
        for(size_t i = 0 ; i < _size; ++i)
            // do NOT memcpy here, or else all breaks if E is a self_relative_ptr
            newData[i] = std::move(_data[i]);
        pmem::obj::delete_persistent<E[]>(_data, _capacity / 2);
        _data = newData;
    }
};

#undef CHECK_IN_TRANSACTION
