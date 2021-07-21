#pragma once

#include <stdexcept>

#include <libpmemobj.h>

#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/pexceptions.hpp>
#include <libpmemobj++/experimental/self_relative_ptr.hpp>
#include <libpmemobj++/make_persistent.hpp>

#define CHECK_IN_TRANSACTION if(pmemobj_tx_stage()!=TX_STAGE_WORK) throw pmem::transaction_scope_error("Operation executed outside transaction");

template <typename T>
class linkedqueue_persistent {
    struct element {
        template <typename... Args>
        element(Args... a) : value(std::forward<Args>(a)...){};
        
        T value;
        pmem::obj::experimental::self_relative_ptr<element> next {nullptr};
    };
    
    pmem::obj::experimental::self_relative_ptr<element> head {nullptr};
    pmem::obj::experimental::self_relative_ptr<element> tail {nullptr};
    pmem::obj::p<size_t> elementCount {0};
    
    struct iterator_base {
        iterator_base(const pmem::obj::experimental::self_relative_ptr<element> * start) : current(start){}
        
        const pmem::obj::experimental::self_relative_ptr<element> * current;
        
        iterator_base & operator ++() {current = &(*current)->next; return *this;}
        bool operator !=(const iterator_base & o) const {
            if(o.current==current) [[unlikely]] return false;
            if(o.current==nullptr) [[likely]] {
                if(current->get()==nullptr) [[unlikely]]
                    return false;
                return true;
            }
            if(current==nullptr){
                if(o.current->get()==nullptr)
                    return false;
                return true;
            }
            return o.current->get()!=current->get();
        }
    };
    struct const_iterator : public iterator_base {
        const_iterator(const pmem::obj::experimental::self_relative_ptr<element> * start) : iterator_base(start){}
        const T & operator * () const {return (*this->current)->value;}
        const T * operator ->() const {return &this->operator*();}
    };
    struct iterator : public const_iterator {
        iterator(const pmem::obj::experimental::self_relative_ptr<element> * start) : const_iterator(start){}
        T & operator * () {pmem::detail::conditional_add_to_tx(&(*this->current)->value); return (*this->current)->value;}
        T * operator ->() const {return &(*this->current)->value;}
    };
    
    
public:
    linkedqueue_persistent() = default;
    template <typename... Args>
    void push_back (Args... v) {
        CHECK_IN_TRANSACTION
        if(head == nullptr){
            head = pmem::obj::make_persistent<element>(std::forward<Args>(v)...);
            tail = head;
        } else {
            tail->next = pmem::obj::make_persistent<element>(std::forward<Args>(v)...);
            tail = tail->next;
        }
        elementCount.get_rw()++;
    }
    void clear() {
        CHECK_IN_TRANSACTION
        pmem::obj::experimental::self_relative_ptr<element> next, curr = head;
        while(curr) {
            next = curr->next;
            pmem::obj::delete_persistent<element>(curr);
            curr = next;
        }
        head = nullptr;
        tail = nullptr;
        elementCount.get_rw()=0;
    }
    
    T pop_front(){
        pmem::obj::p<T> e;
        if(head==nullptr)
            throw "Removing from an empty list";
        
        // when removing last element, tail needs to be set to nullptr as well
        if(head==tail) tail = nullptr;
        
        e = std::move(head->value);
        auto next = head->next;
        pmem::obj::delete_persistent<element>(head);
        head = next;
        elementCount.get_rw()--;
        return e;
    }
    
    const T& front() const {
        if(head==nullptr)
            throw "Reading head of an empty list";
        return head->value;
    }
    
    T& front() {
        if(head==nullptr)
            throw "Reading head of an empty list";
        pmem::detail::conditional_add_to_tx(&head->value);
        return head->value;
    }
    
    size_t count() const {return elementCount.get_ro();}
    bool empty() const {return elementCount==0;}
    
    iterator begin() {return iterator(&head);}
    const_iterator begin() const {return iterator(&head);}
    
    iterator end() {return iterator(nullptr);}
    const_iterator end() const {return iterator(nullptr);}
};

namespace pmem::detail
{
    template <typename T>
    struct can_do_snapshot<linkedqueue_persistent<T>> {
        static constexpr bool value = can_do_snapshot<T>::value;
    };
}

#undef CHECK_IN_TRANSACTION
