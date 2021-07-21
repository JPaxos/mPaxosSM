#ifndef SIMPLE_KV_SERVICE_H_INCLUDED
#define SIMPLE_KV_SERVICE_H_INCLUDED

#include <jni.h>

#include <bit>
#include <string>
#include <string_view>
#include <array>
#include <unordered_map>

using namespace std::literals;

#undef DEBUG_REQUESTS

#define SHA512_DIGEST_LENGTH 64

std::tuple<const char *, size_t, bool> execute(JNIEnv * env, long requestSequentialNumber, const char * request, size_t len);

std::string getSnapshotFile();

void releaseSnapshotFile();

void updateToSnapshot(JNIEnv * env, std::string filename);

inline uint32_t fromBE(uint32_t in){
    if constexpr (std::endian::native == std::endian::little){
        std::swap(*(((uint8_t*)&in)+0), *(((uint8_t*)&in)+3));
        std::swap(*(((uint8_t*)&in)+1), *(((uint8_t*)&in)+2));
    }
    return in;
}

#endif // SIMPLE_KV_SERVICE_H_INCLUDED
