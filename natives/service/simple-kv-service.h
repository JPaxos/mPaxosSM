#ifndef SIMPLE_KV_SERVICE_H_INCLUDED
#define SIMPLE_KV_SERVICE_H_INCLUDED

#include <jni.h>
#include <string>
#include <string_view>
#include <bit>

#include <libpmemobj++/container/string.hpp>
#include <libpmemobj++/container/vector.hpp>

using namespace std::literals;

#include "NvmHashMap.hpp"
#include "NvmHashMap_helpers.hpp"

#undef DEBUG_REQUESTS

namespace pm = pmem::obj;

#define SHA512_DIGEST_LENGTH 64

struct root {
    NvmHashMap<pm::string, pm::string, false, MyHash, MyComparator, MyFactory, MyFactory> kvmap;
    pm::p<long> lastRequest = {-1};
    pm::array<unsigned char, SHA512_DIGEST_LENGTH> lastSha = {};
};

extern pm::pool<root> *pop;

void onFirstRunEver();

void onEachPmemFileOpen();

std::tuple<const char *, size_t, bool> execute(JNIEnv * env, long requestSequentialNumber, const char * request, size_t len);

inline uint32_t fromBE(uint32_t in){
    if constexpr (std::endian::native == std::endian::little){
        std::swap(*(((uint8_t*)&in)+0), *(((uint8_t*)&in)+3));
        std::swap(*(((uint8_t*)&in)+1), *(((uint8_t*)&in)+2));
    }
    return in;
}

#endif // SIMPLE_KV_SERVICE_H_INCLUDED
