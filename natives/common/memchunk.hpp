#pragma once

#include <compare>
#include <libpmemobj++/utils.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/experimental/self_relative_ptr.hpp>
namespace pm = pmem::obj;

/// Wrapper of a memory block that stores a pointer to the block and its size

struct MemChunk {
    pm::experimental::self_relative_ptr<char[]> value;
    pm::p<size_t> length;
    
    MemChunk() : value(nullptr), length(0){}
    
    template <typename T>
    MemChunk(pm::pool_base&& pop, const T & var) {assign(pop, var.data(), var.size());}
    
    MemChunk(const MemChunk & var) = delete;
    
    // line below enables use of MemChunk in pmem::obj::concurrent_hash_map
    // by making it constructable from i.e. std::string or vector/array of char
    template <typename T>
    MemChunk(const T & var) {assign(pm::pool_by_vptr(this), var.data(), var.size());}
    
    void assign(pm::pool_base& pop, const void * mem, size_t len){
        value = pm::make_persistent<char[]>(len);
        length = len;
        pop.memcpy_persist(value.get(), mem, len);
    }
    
    void assign(pm::pool_base&& pop, const void * mem, size_t len){
        assign(pop, mem, len);
    }
    
    void overwrite(pm::pool_base& pop, const void * mem, size_t len){
        if(len!=length){
            free();
            assign(pop, mem, len);
        } else {
            pm::transaction::snapshot(value.get(), length);
            pop.memcpy_persist(value.get(), mem, len);
        }
    }
    
    void overwrite(pm::pool_base&& pop, const void * mem, size_t len){
        overwrite(pop, mem, len);
    }
    
    void free() {
        if (value) {
            pm::delete_persistent<char[]>(value, length);
            value = nullptr;
            length = 0;
        }
    }
    
    template <typename T>
    MemChunk& operator=(const T & var) {
        assign(pm::pool_by_vptr(this), var.data(), var.size());
        return *this;
    }
    
    operator bool() const {
        return (bool) value;
    }
    
    bool operator !=(const MemChunk & other) const {
        auto whosLonger = length <=> other.length;
        if(whosLonger==0) return true;
        auto lexical = memcmp(value.get(), other.value.get(), whosLonger > 0 ? length : other.length);
        return lexical;
    }
    
    auto operator <=>(const MemChunk & other) const {
        auto whosLonger = length <=> other.length;
        auto lexical = memcmp(value.get(), other.value.get(), whosLonger > 0 ? length : other.length);
        if(lexical) return lexical>0 ? std::strong_ordering::greater : std::strong_ordering::less;
        return whosLonger;
    }
};

namespace pmem::detail
{
    template <>
    struct can_do_snapshot<MemChunk> {
        static constexpr bool value = true;
    };
}


struct MemChunkComp;

template<> struct std::hash<MemChunk>{
    auto operator()(const MemChunk & chunk) const {
        return std::hash<std::string_view>{}({chunk.value.get(), chunk.length});
    }
    
    template <typename T>
    auto operator()(const T & anything) const {
        return std::hash<std::string_view>{}(anything);
    }
    
    // line below enables use of MemChunk in pmem::obj::concurrent_hash_map by
    // allowing to use std::string(_view) as keys.
    typedef MemChunkComp transparent_key_equal;
};

struct MemChunkComp {
    bool operator()(const MemChunk & a, const MemChunk & b) const {
        return !(a!=b);
        //return a<=>b == std::strong_ordering::equal;
    }
    
    template <typename T>
    bool operator()(const MemChunk & chunk, const T & something) const {
        return something == std::string_view(chunk.value.get(), chunk.length);
    }
    
    // this is for pmem::obj::concurrent_hash_map, which has wrong arg order
    template <typename T>
    bool operator()(const T & something, const MemChunk & chunk) const {
        return operator()(chunk, something);
    }
};

struct MemChunkMaker {
    MemChunk& operator()(MemChunk& dst, const MemChunk& src) {
        return dst=src;
    }
    
    MemChunk& operator()(MemChunk& dst, const char* src, size_t length) {
        dst.assign(pm::pool_by_vptr(&dst), src, length);
        return dst;
    }
    
    template <typename T>
    MemChunk& operator()(MemChunk& dst, const T& src) {
        return operator()(dst, src.data(), src.size());
    }
    
    MemChunk& operator()(MemChunk& m){
        return m;
    }
    
    const MemChunk& operator()(const MemChunk& m){
        return m;
    }
};
