#include <unistd.h>
#include <type_traits>

#include "simple-kv-service.h"

#include "jpaxos-service.h"
extern Config cfg;
extern jint localId;

#ifdef DEBUG_REQUESTS
#include <cstdio>
#endif

#ifndef NDEBUG
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cassert>
#include <fcntl.h>
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

        
void onFirstRunEver(){
    pm::transaction::run(*pop, [&]{
        new (pop->root().get()) std::remove_reference_t<decltype(*(pop->root().get()))>; 
    });
}

void onEachPmemFileOpen(){
}

std::tuple<const char *, size_t, bool> execute(JNIEnv * env, long seqNo, const char * request, size_t len){
    if(len < 5)
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "Too short request");
    
    auto & kvmap = pop->root()->kvmap;
    
    char reqType = request[0];
    uint32_t keyLen = fromBE(*(uint32_t*)(request+1));
    
    if(len < 5 + keyLen)
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "Key length exceeds available data");
    
    std::string_view key(request+5, keyLen);
    
    #ifdef DEBUG_REQUESTS
    printf("SERVICE: execute %ld %c %02hhx%02hhx%02hhx%02hhx %lu\n", ssn, reqType, *(request+5), *(request+6), *(request+7), *(request+8), len);
    fflush(stdout);
    #endif

    const char * response = nullptr;
    size_t resposeLength = 0;
    bool shallDelete = false;
    
    switch(reqType){
        case 'G': {
            if(len != 5 + keyLen)
                env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "Surplus data past key");
                
            try{
                auto value = kvmap.get<std::string_view>(key);
                response = value.data();
                resposeLength = value.length();
            } catch(no_such_key_exception & ex){
                // default {nullptr, 0, false} are fine here
            }
        break;
        }
        case 'P': {
            std::string_view value(request + 5 - keyLen, len - 5 - keyLen);
            
            try{
                auto value = kvmap.get<std::string_view>(key);
                char * oldVal = new char [value.length()];
                memcpy(oldVal, value.data(), value.length());
                
                response = oldVal;
                resposeLength = value.length();
                shallDelete = true;
            } catch(no_such_key_exception & ex){
                // default {nullptr, 0, false} are fine here
            }
            
            kvmap.insertOrReplace(*pop, key, value);
            
        break;
        }
        default:
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), 
                          ("Unknown request type: ["s + reqType + ']').c_str());
    }
    
    // FIX ME: put at recovery can now return a different result.
    
    #ifndef NDEBUG
        if(pop->root()->lastRequest == seqNo) {
            /* Intentionally empty.
            * 
            * This branch can occur upon recovery, when the crash happened
            * after the request has been passed to the service, but before
            * the response was returned to JPaxos.
            */
        } else {
            pm::transaction::automatic tx(*pop);
                
            pop->root()->lastRequest.get_rw()++;
            assert(pop->root()->lastRequest == seqNo);
        
            static FILE* requestLog = openRequestLog();
            
            auto & lastSha = pop->root()->lastSha;
            
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
        }
    #endif
    
    return {response, resposeLength, shallDelete};
}
