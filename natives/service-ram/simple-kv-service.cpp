#include <unistd.h>
#include <type_traits>
#include <cstring>
#include <fcntl.h>

#include "simple-kv-service.h"

#include "jpaxos-service.h"
extern Config cfg;
extern jint localId;

#include "backtrace_error.h"

#ifdef DEBUG_REQUESTS
#include <cstdio>
#endif

#ifndef NDEBUG
#include <sys/stat.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cassert>
FILE* openRequestLog() {
    mkdir("jpaxosLogs", 0777);
    auto dir = "jpaxosLogs/" + std::to_string(localId);
    mkdir(dir.c_str(), 0777);
    struct stat st;
    if(stat(dir.c_str(), &st))
        assert(false);
    if(!S_ISDIR(st.st_mode))
        assert(false);
    
    auto filename = dir + "/decisions.log.";
    int runId = 0;
    while(!access((filename+std::to_string(runId)).c_str(), F_OK))
        runId++;
    
    int fd = open((filename+std::to_string(runId)).c_str(), O_WRONLY|O_CREAT|O_EXCL, 0666);
    assert(fd != -1);
    
    return fdopen(fd, "w");
}
#endif

std::unordered_map<std::string, std::string> kvmap;
long lastRequest = {-1};
std::array<unsigned char, SHA512_DIGEST_LENGTH> lastSha = {};

std::tuple<const char *, size_t, bool> execute(JNIEnv * env, long seqNo, const char * request, size_t len){
    if(len < 5)
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "Too short request");
    
    char reqType = request[0];
    uint32_t keyLen = fromBE(*(uint32_t*)(request+1));
    
    if(len < 5 + keyLen)
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "Key length exceeds available data");
    
    std::string key(request+5, keyLen);
    
    #ifdef DEBUG_REQUESTS
    printf("SERVICE: execute %ld %c %02hhx%02hhx%02hhx%02hhx %lu\n", seqNo, reqType, *(request+5), *(request+6), *(request+7), *(request+8), len);
    fflush(stdout);
    #endif

    const char * response = nullptr;
    size_t resposeLength = 0;
    bool shallDelete = false;
    
    switch(reqType){
        case 'G': {
            if(len != 5 + keyLen)
                env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "Surplus data past key");
            
            auto it = kvmap.find(key);
            if(it!=kvmap.end()){
                response = it->second.data();
                resposeLength = it->second.length();
            } else {
                // default {nullptr, 0, false} are fine here
            }
        break;
        }
        case 'P': {
            // std::string_view value(request + 5 - keyLen, len - 5 - keyLen);
            
            auto it = kvmap.find(key);
            if(it!=kvmap.end()){
                auto & value = it->second;
                char * oldVal = new char [value.length()];
                memcpy(oldVal, value.data(), value.length());
                
                response = oldVal;
                resposeLength = value.length();
                shallDelete = true;
                
                it->second = std::string(request + 5 - keyLen, len - 5 - keyLen);
            } else {
                kvmap[key] = std::string(request + 5 - keyLen, len - 5 - keyLen);
                // default {nullptr, 0, false} are fine here
            }
        break;
        }
        default:
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), 
                          ("Unknown request type: ["s + reqType + ']').c_str());
    }
    
    if(lastRequest == seqNo) {
        /* Intentionally empty.
         * 
         * This branch can occur upon recovery, when the crash happened
         * after the request has been passed to the service, but before
         * the response was returned to JPaxos.
         */
    } else {
        lastRequest++;
        #ifndef NDEBUG
	         assert(lastRequest == seqNo);
            
            static FILE* requestLog = openRequestLog();
            
            SHA512_CTX ctx;
            SHA512_Init(&ctx);
            SHA512_Update(&ctx, lastSha.data(), lastSha.size());
            SHA512_Update(&ctx, request, len);
            SHA512_Final(lastSha.data(), &ctx);
            
            BIO * buffer = BIO_new(BIO_s_mem());
            BIO * b64 = BIO_push(BIO_new(BIO_f_base64()), buffer);
            BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
            BIO_write(b64, lastSha.data(), lastSha.size());
            BIO_flush(b64);
            BIO_write(buffer, "", 1);
            BUF_MEM * bufferPtr;
            BIO_get_mem_ptr(buffer, &bufferPtr);
            
            fprintf(requestLog, "%ld %s\n", seqNo, bufferPtr->data);
            fflush(requestLog);
            
            BIO_free(buffer);
        #endif
    }
    
    return {response, resposeLength, shallDelete};
}



void releaseSnapshotFile() {
    std::string fn = cfg.path + "." + std::to_string(localId);
    if(-1==unlink(fn.c_str()))
        throw backtrace_error("cannot unlink snapshot file");
}

void writeT(int fd, const void* data, ssize_t len){
    if (len != write(fd, data, len))
        throw backtrace_error("writeT failed");
}

void readT(int fd, void* data, ssize_t len){
    if (len != read(fd, data, len))
        throw backtrace_error("readT failed");
}

std::string getSnapshotFile() {
    std::string fn = cfg.path + "." + std::to_string(localId);
    int fd = open(fn.c_str(), O_TRUNC|O_CREAT, 0666);
    if(-1 == fd)
        throw backtrace_error("cannot create snapshot file");
    
    writeT(fd, &lastRequest, sizeof(lastRequest));
    writeT(fd, lastSha.data(), lastSha.size());
    
    int t;
    
    t=kvmap.size();
    writeT(fd, &t, sizeof(t));
    for(auto const & [key, value] : kvmap){
        t = key.length();
        writeT(fd, &t, sizeof(t));
        writeT(fd, key.c_str(), t);
        
        t = value.length();
        writeT(fd, &t, sizeof(t));
        writeT(fd, value.c_str(), t);
    }
    
    close(fd);
    return fn;
}


void updateToSnapshot(JNIEnv *, std::string filename){
    int fd = open(filename.c_str(), O_RDONLY);
    if(-1 == fd)
        throw backtrace_error("cannot open snapshot file");
    
    readT(fd, &lastRequest, sizeof(lastRequest));
    readT(fd, lastSha.data(), lastSha.size());
    
    int kvsize;
    
    readT(fd, &kvsize, sizeof(kvsize));
    kvmap.clear();
    kvmap.reserve(kvsize);
    
    for(int i = 0; i < kvsize; ++i){
        int keylen, vallen;
        std::string key, value;
        
        readT(fd, &keylen, sizeof(keylen));
        key.resize(keylen);
        readT(fd, key.data(), keylen);
        
        readT(fd, &vallen, sizeof(vallen));
        value.resize(vallen);
        readT(fd, value.data(), vallen);
        
        kvmap.emplace(key, value);
    }
    
    close(fd);
    
}
